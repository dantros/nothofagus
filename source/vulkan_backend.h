#pragma once

#include "render_backend.h"
#include "vulkan_mesh.h"
#include "vulkan_texture.h"
#include "vulkan_render_target.h"
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <array>
#include <vector>
#include <cstdint>

// Forward-declare VmaAllocator to avoid including vk_mem_alloc.h in every TU.
struct VmaAllocator_T;
typedef VmaAllocator_T* VmaAllocator;

namespace Nothofagus
{

static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

struct PendingBufferDeletion
{
    VkBuffer      buffer;
    VmaAllocation allocation;
};

struct FrameData
{
    VkCommandBuffer commandBuffer  = VK_NULL_HANDLE;
    VkSemaphore     imageAvailable = VK_NULL_HANDLE;
    VkSemaphore     renderFinished = VK_NULL_HANDLE;
    VkFence         inFlight       = VK_NULL_HANDLE;

    // Buffers queued for deletion — flushed at the start of the next beginFrame()
    // for this slot, after vkWaitForFences guarantees the GPU is done with them.
    std::vector<PendingBufferDeletion> pendingBufferDeletions;
};

/// Push constant layout for sprite drawing.
/// mat3 columns are padded to vec4 in Vulkan GLSL — 48 bytes, not 36.
/// Never memcpy a glm::mat3 directly: copy column-by-column.
struct SpritePushConstants
{
    float col0[3]; float _pad0;   // 16 bytes
    float col1[3]; float _pad1;   // 16 bytes
    float col2[3]; float _pad2;   // 16 bytes
    int   layerIndex;             //  4 bytes
    float tintColor[3];           // 12 bytes
    float tintIntensity;          //  4 bytes
    float opacity;                //  4 bytes
};
static_assert(sizeof(SpritePushConstants) == 72);

class VulkanBackend
{
public:
    // --- RenderBackend concept interface ---

    void initialize(GLFWwindow* window, glm::ivec2 canvasSize);
    void shutdown();

    DTexture      uploadTexture(const Texture& texture, TextureSampleMode minFilter, TextureSampleMode magFilter);
    void          freeTexture(DTexture dtexture);
    DMesh         uploadMesh(const Mesh& mesh);
    void          freeMesh(DMesh dmesh);
    DRenderTarget createRenderTarget(glm::ivec2 size);
    DTexture      getRenderTargetTexture(DRenderTarget renderTarget);
    void          freeRenderTarget(DRenderTarget renderTarget, DTexture proxyTexture);

    void beginFrame(glm::vec3 clearColor, ViewportRect gameViewport, int framebufferWidth, int framebufferHeight);
    void imguiNewFrame();
    void beginRttPass(DRenderTarget renderTarget, glm::vec4 clearColor);
    void endRttPass();
    void beginMainPass(ViewportRect gameViewport);
    void drawSprite(DMesh dmesh, DTexture dtexture, const SpriteDrawParams& params);
    void endFrame(GLFWwindow* window, ImDrawData* imguiData, int framebufferWidth, int framebufferHeight);
    ScreenshotPixels takeScreenshot(ViewportRect gameViewport, glm::ivec2 gameSize) const;

    // Not part of the concept — called by canvas_impl to update filter after upload
    void setTextureMinFilter(DTexture dtexture, TextureSampleMode mode);
    void setTextureMagFilter(DTexture dtexture, TextureSampleMode mode);

private:
    // --- Device objects ---
    VkInstance               mInstance            = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT mDebugMessenger       = VK_NULL_HANDLE;
    VkSurfaceKHR             mSurface              = VK_NULL_HANDLE;
    mutable VkPhysicalDevice mPhysicalDevice       = VK_NULL_HANDLE;
    mutable VkDevice         mDevice               = VK_NULL_HANDLE;
    mutable VkQueue          mGraphicsQueue        = VK_NULL_HANDLE;
    VkQueue                  mPresentQueue         = VK_NULL_HANDLE;
    uint32_t                 mGraphicsQueueFamily  = 0;

    // --- Swapchain ---
    VkSwapchainKHR             mSwapchain            = VK_NULL_HANDLE;
    VkFormat                   mSwapchainFormat      = VK_FORMAT_UNDEFINED;
    VkExtent2D                 mSwapchainExtent      = {};
    std::vector<VkImage>       mSwapchainImages;
    std::vector<VkImageView>   mSwapchainImageViews;
    std::vector<VkFramebuffer> mSwapchainFramebuffers;

    // Shared depth image for swapchain framebuffers
    VkImage       mSwapchainDepthImage = VK_NULL_HANDLE;
    VmaAllocation mSwapchainDepthAlloc = VK_NULL_HANDLE;
    VkImageView   mSwapchainDepthView  = VK_NULL_HANDLE;

    // Depth format shared by both render passes (D24_UNORM_S8_UINT or D32_SFLOAT fallback)
    VkFormat mDepthFormat = VK_FORMAT_UNDEFINED;

    // --- Render passes ---
    VkRenderPass mMainRenderPass = VK_NULL_HANDLE;  // swapchain output
    VkRenderPass mRttRenderPass  = VK_NULL_HANDLE;  // off-screen output

    // --- Pipeline ---
    VkDescriptorSetLayout mDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout      mPipelineLayout      = VK_NULL_HANDLE;
    VkPipeline            mSpritePipeline      = VK_NULL_HANDLE;

    // --- Descriptor pools ---
    VkDescriptorPool mDescriptorPool      = VK_NULL_HANDLE;  // per-texture combined image sampler
    VkDescriptorPool mImguiDescriptorPool = VK_NULL_HANDLE;

    // --- Commands + per-frame sync ---
    mutable VkCommandPool mCommandPool = VK_NULL_HANDLE;
    std::array<FrameData, MAX_FRAMES_IN_FLIGHT> mFrames;
    int mCurrentFrame = 0;

    // --- Memory allocator ---
    mutable VmaAllocator mAllocator = VK_NULL_HANDLE;

    // --- GPU resource maps ---
    std::unordered_map<std::size_t, VulkanMesh>         mMeshes;
    std::unordered_map<std::size_t, VulkanTexture>      mTextures;
    std::unordered_map<std::size_t, VulkanRenderTarget> mRenderTargets;
    std::size_t mNextId = 0;

    // --- Per-frame state (set in beginFrame/beginRttPass, consumed by draw calls and endFrame) ---
    glm::vec3       mClearColor                 = {};
    uint32_t        mCurrentSwapchainImageIndex = 0;
    VkCommandBuffer mActiveCommandBuffer        = VK_NULL_HANDLE;
    ViewportRect    mCurrentGameViewport        = {};
    int             mCurrentFramebufferWidth    = 0;
    int             mCurrentFramebufferHeight   = 0;

    // Active RTT render target id (0 = none), stored by beginRttPass, cleared by endRttPass
    std::size_t mActiveRttRenderTargetId = 0;

    // --- Private helpers ---
    VkCommandBuffer beginOneTimeCommandBuffer() const;
    void            endOneTimeCommandBuffer(VkCommandBuffer commandBuffer) const;

    void transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image,
                               VkImageLayout oldLayout, VkImageLayout newLayout,
                               VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
                               VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                               uint32_t layerCount = 1) const;

    VkShaderModule  createShaderModule(const char* spirvPath) const;
    VkSampler       createSampler(TextureSampleMode minFilter, TextureSampleMode magFilter) const;
    VkDescriptorSet allocateAndUpdateDescriptorSet(VkImageView imageView, VkSampler sampler) const;
    VkFormat        findDepthFormat() const;

    void createSwapchainDepthResources();
    void destroySwapchainDepthResources();
    void createSwapchainFramebuffers();
    void destroySwapchainResources();
    void recreateSwapchain(GLFWwindow* window);

    void rebuildSampler(VulkanTexture& tex, TextureSampleMode minFilter, TextureSampleMode magFilter);
};

static_assert(RenderBackend<VulkanBackend>,
    "VulkanBackend does not satisfy the RenderBackend concept");

} // namespace Nothofagus
