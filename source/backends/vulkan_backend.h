#pragma once

#include "render_backend.h"
#include "vulkan_mesh.h"
#include "vulkan_texture.h"
#include "vulkan_render_target.h"
#include "vulkan_presentation.h"
#include <vulkan/vulkan.h>
#ifdef TRACY_ENABLE
#include <tracy/TracyVulkan.hpp>
#else
using TracyVkCtx = void*;
#define TracyVkContext(x,y,z,w) nullptr
#define TracyVkDestroy(x)
#define TracyVkZone(c,x,y)
#define TracyVkCollect(c,x)
#endif
#include <unordered_map>
#include <array>
#include <vector>
#include <span>
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

struct PendingTextureDeletion
{
    VkDescriptorSet descriptorSet;
    VkSampler       sampler;
    VkImageView     imageView;
    VkImage         image;       // VK_NULL_HANDLE for proxy textures (image owned by RT)
    VmaAllocation   allocation;  // nullptr for proxy textures

    // Palette resources (only populated for indirect textures).
    VkSampler       paletteSampler    = VK_NULL_HANDLE;
    VkImageView     paletteImageView  = VK_NULL_HANDLE;
    VkImage         paletteImage      = VK_NULL_HANDLE;
    VmaAllocation   paletteAllocation = VK_NULL_HANDLE;

    // Map resources (only populated for tile-map textures — separate map DTexture entry).
    // These are unused in the atlas entry's deletion record; the map entry uses the
    // standard fields (image / allocation / imageView / sampler) for its own GPU resources.
};


struct PendingRenderTargetDeletion
{
    // Proxy texture fields (descriptor set + sampler; image view is the RT's colorView)
    VkDescriptorSet proxyDescriptorSet;
    VkSampler       proxySampler;
    // Render target fields
    VkFramebuffer framebuffer;
    VkImageView   colorView;
    VkImageView   depthView;
    VkImage       colorImage;
    VmaAllocation colorAlloc;
    VkImage       depthImage;
    VmaAllocation depthAlloc;
};

struct FrameData
{
    VkCommandBuffer commandBuffer  = VK_NULL_HANDLE;
    VkFence         inFlight       = VK_NULL_HANDLE;

    // Resources queued for deletion — flushed at the start of the next beginFrame()
    // for this slot, after vkWaitForFences guarantees the GPU is done with them.
    std::vector<PendingBufferDeletion>       pendingBufferDeletions;
    std::vector<PendingTextureDeletion>      pendingTextureDeletions;
    std::vector<PendingRenderTargetDeletion> pendingRenderTargetDeletions;
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

    void initialize(void* nativeWindowHandle, glm::ivec2 canvasSize);
    void initImGuiRenderer();
    void shutdown();

    DTexture      uploadTexture(const Texture& texture, TextureSampleMode minFilter, TextureSampleMode magFilter);
    void          freeTexture(DTexture dtexture);
    DTexture      uploadPaletteTexture(const std::vector<glm::vec4>& paletteColors);
    void          updatePaletteTexture(DTexture paletteTexture, const std::vector<glm::vec4>& paletteColors);
    void          freePaletteTexture(DTexture paletteTexture);
    void          linkIndirectTextures(DTexture indexTexture, DTexture paletteTexture);
    DTexture      uploadTileMapTexture(std::span<const std::uint8_t> mapData, glm::ivec2 mapSize);
    void          freeTileMapTexture(DTexture mapTexture);
    void          linkTileMapTextures(DTexture atlasTexture, DTexture mapTexture);
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
    void endFrame(ImDrawData* imguiData, int framebufferWidth, int framebufferHeight);

    // ImGui-to-render-target: each secondary ImGuiContext gets its own
    // ImGui_ImplVulkan backend instance, initialized against mRttRenderPass so the
    // pipeline is compatible with beginRttPass. Called with that context active.
    void initImguiForRenderTarget(DRenderTarget renderTarget);
    void shutdownImguiForRenderTarget(DRenderTarget renderTarget);
    void imguiNewFrameForRenderTarget(DRenderTarget renderTarget);
    void renderImguiDrawDataToRenderTarget(ImDrawData* imguiData, DRenderTarget renderTarget);

    ScreenshotPixels takeScreenshot(ViewportRect gameViewport, glm::ivec2 gameSize) const;

    // Not part of the concept — called by canvas_impl to update filter after upload
    void setTextureMinFilter(DTexture dtexture, TextureSampleMode mode);
    void setTextureMagFilter(DTexture dtexture, TextureSampleMode mode);

private:
    // --- Presentation policy (windowed or headless, selected at compile time) ---
    ActiveVulkanPresentation mPresentation;

    // --- Tracy GPU profiling context ---
    TracyVkCtx mTracyVkContext = nullptr;

    // --- Device objects ---
    VkInstance               mInstance            = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT mDebugMessenger       = VK_NULL_HANDLE;
    mutable VkPhysicalDevice mPhysicalDevice       = VK_NULL_HANDLE;
    mutable VkDevice         mDevice               = VK_NULL_HANDLE;
    mutable VkQueue          mGraphicsQueue        = VK_NULL_HANDLE;
    uint32_t                 mGraphicsQueueFamily  = 0;

    // Depth format shared by both render passes (D24_UNORM_S8_UINT or D32_SFLOAT fallback)
    VkFormat mDepthFormat = VK_FORMAT_UNDEFINED;

    // --- Render passes ---
    VkRenderPass mMainRenderPass = VK_NULL_HANDLE;  // main output (windowed: swapchain, headless: offscreen)
    VkRenderPass mRttRenderPass  = VK_NULL_HANDLE;  // off-screen render-to-texture

    // --- Pipeline (direct textures) ---
    VkDescriptorSetLayout mDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout      mPipelineLayout      = VK_NULL_HANDLE;
    VkPipeline            mSpritePipeline      = VK_NULL_HANDLE;

    // --- Pipeline (indirect / palette-based textures) ---
    VkDescriptorSetLayout mIndirectDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout      mIndirectPipelineLayout      = VK_NULL_HANDLE;
    VkPipeline            mIndirectSpritePipeline      = VK_NULL_HANDLE;

    // --- Pipeline (tile-map textures) ---
    VkDescriptorSetLayout mTilemapDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout      mTilemapPipelineLayout      = VK_NULL_HANDLE;
    VkPipeline            mTilemapSpritePipeline      = VK_NULL_HANDLE;

    // --- Descriptor pools ---
    VkDescriptorPool mDescriptorPool      = VK_NULL_HANDLE;  // per-texture combined image sampler
    VkDescriptorPool mImguiDescriptorPool = VK_NULL_HANDLE;

    // One ImGui descriptor pool per render target that hosts a secondary ImGuiContext.
    // ImGui_ImplVulkan stores its state (pipeline + descriptor sets) per-context, so
    // each context needs its own pool. Keyed by DRenderTarget::id.
    std::unordered_map<std::size_t, VkDescriptorPool> mRttImguiDescriptorPools;

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
    glm::vec3       mClearColor              = {};
    VkCommandBuffer mActiveCommandBuffer     = VK_NULL_HANDLE;
    ViewportRect    mCurrentGameViewport     = {};
    int             mCurrentFramebufferWidth  = 0;
    int             mCurrentFramebufferHeight = 0;

    // Active RTT render target id (0 = none), stored by beginRttPass, cleared by endRttPass
    std::size_t mActiveRttRenderTargetId = 0;

    // --- Private helpers ---
    void flushPendingDeletions(FrameData& frame);

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
    VkDescriptorSet allocateAndUpdateIndirectDescriptorSet(
        VkImageView indexView, VkSampler indexSampler,
        VkImageView paletteView, VkSampler paletteSampler) const;
    VkDescriptorSet allocateAndUpdateTilemapDescriptorSet(
        VkImageView atlasView, VkSampler atlasSampler,
        VkImageView mapView,   VkSampler mapSampler) const;
    VkFormat        findDepthFormat() const;

    void rebuildSampler(VulkanTexture& tex, TextureSampleMode minFilter, TextureSampleMode magFilter);
};

static_assert(RenderBackend<VulkanBackend>,
    "VulkanBackend does not satisfy the RenderBackend concept");

} // namespace Nothofagus
