#pragma once

#include "render_backend.h"
#include <vulkan/vulkan.h>
#include <glm/vec2.hpp>
#include <vector>
#include <cstdint>

// Forward-declare VMA types to avoid including vk_mem_alloc.h in every TU.
struct VmaAllocator_T;
typedef VmaAllocator_T* VmaAllocator;
struct VmaAllocation_T;
typedef VmaAllocation_T* VmaAllocation;

// Forward-declare vk-bootstrap types used in the policy interface.
namespace vkb { class PhysicalDeviceSelector; struct Device; }

// Forward-declare GLFWwindow outside any namespace so it matches the GLFW header (used by GLFW backend).
#if !defined(NOTHOFAGUS_HEADLESS_VULKAN) && !defined(NOTHOFAGUS_BACKEND_SDL3)
struct GLFWwindow;
#endif

namespace Nothofagus
{

/// Result of the per-frame image acquisition step.
enum class AcquireResult
{
    Success,     ///< Image acquired (or always available in headless mode).
    Recreated,   ///< Swapchain was recreated — caller should skip this frame.
};

// ---------------------------------------------------------------------------
// The active presentation policy is selected at compile time.  Only the
// chosen policy struct is defined; the other is excluded via #ifdef so
// there is zero overhead from the unused path.
// ---------------------------------------------------------------------------

#ifndef NOTHOFAGUS_HEADLESS_VULKAN

// ---------------------------------------------------------------------------
// WindowedVulkanPresentation
// ---------------------------------------------------------------------------

/// Presentation policy for windowed rendering via a VkSurfaceKHR + VkSwapchainKHR.
struct WindowedVulkanPresentation
{
    // --- Initialization (two-phase: target determines format, framebuffers need render pass) ---
    void createSurface(VkInstance instance, void* nativeWindowHandle);
    void configurePhysicalDeviceSelector(vkb::PhysicalDeviceSelector& selector);
    void retrieveQueues(vkb::Device& vkbDevice,
                        VkQueue& outGraphicsQueue, uint32_t& outGraphicsQueueFamily);
    void createPresentationTarget(VkPhysicalDevice physicalDevice, VkDevice device,
                                  VmaAllocator allocator, VkFormat depthFormat,
                                  glm::ivec2 canvasSize, VkPresentModeKHR presentMode);
    void createPresentationFramebuffers(VkRenderPass mainRenderPass);
    void createSyncObjects(VkDevice device);

    // --- Render pass configuration ---
    VkFormat      colorFormat()        const;
    VkImageLayout mainPassFinalLayout() const;
    uint32_t      imageCount()         const;

    // --- Per-frame ---
    /// Acquire the next swapchain image. If the supplied framebuffer size differs
    /// from the current swapchain extent (window was resized), the swapchain is
    /// recreated up-front so all downstream viewport / scissor math stays consistent
    /// with the surface actually being rendered to.
    AcquireResult acquireImage(VkDevice device,
                               uint32_t framebufferWidth, uint32_t framebufferHeight);
    VkFramebuffer mainFramebuffer()    const;
    VkExtent2D    extent()             const;
    void          submitAndPresent(VkDevice device, VkQueue graphicsQueue,
                                   VkCommandBuffer commandBuffer, VkFence frameFence);

    // --- Screenshot (scheduled: recorded in-frame before present, read back after fence) ---
    void armCapture(glm::ivec2 gameSize);
    void recordCapture(VkCommandBuffer commandBuffer, ViewportRect gameViewport, VkFence frameFence);
    bool captureReady() const;
    ScreenshotPixels finishCapture(VkDevice device, VmaAllocator allocator,
                                   VkCommandPool commandPool, VkQueue graphicsQueue);

    // --- Cleanup ---
    void shutdown(VkDevice device, VmaAllocator allocator, VkInstance instance);

private:
    void recreateSwapchain();
    void destroyCaptureResources();
    ScreenSize queryFramebufferSize() const;
    void createDepthResources();
    void destroyDepthResources();
    void createFramebuffers();
    void destroySwapchainResources();

    // Handles stored during initialization for recreateSwapchain / acquireImage.
    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkDevice         mDevice         = VK_NULL_HANDLE;
    VmaAllocator     mAllocator      = VK_NULL_HANDLE;
    VkFormat         mDepthFormat    = VK_FORMAT_UNDEFINED;
    VkRenderPass     mRenderPass     = VK_NULL_HANDLE;

    // Surface + swapchain state.
    VkPresentModeKHR           mPresentMode      = VK_PRESENT_MODE_FIFO_KHR; // Stored so recreateSwapchain reuses it.
    VkSurfaceKHR               mSurface          = VK_NULL_HANDLE;
    VkSwapchainKHR             mSwapchain        = VK_NULL_HANDLE;
    VkFormat                   mSwapchainFormat  = VK_FORMAT_UNDEFINED;
    VkExtent2D                 mSwapchainExtent  = {};
    std::vector<VkImage>       mSwapchainImages;
    std::vector<VkImageView>   mSwapchainImageViews;
    std::vector<VkFramebuffer> mSwapchainFramebuffers;

    // Shared depth image for swapchain framebuffers.
    VkImage       mDepthImage = VK_NULL_HANDLE;
    VmaAllocation mDepthAlloc = VK_NULL_HANDLE;
    VkImageView   mDepthView  = VK_NULL_HANDLE;

    // Per-swapchain-image semaphores.
    std::vector<VkSemaphore> mImageAvailableSemaphores;
    std::vector<VkSemaphore> mRenderFinishedSemaphores;
    uint32_t                 mAcquireSemaphoreIndex  = 0;
    uint32_t                 mCurrentImageIndex      = 0;

    VkQueue mPresentQueue = VK_NULL_HANDLE;

    // Store the native window handle for framebuffer size queries during
    // swapchain creation/recreation (Wayland doesn't provide currentExtent).
    void* mNativeWindowHandle = nullptr;

    // Deferred screenshot capture: the copy is recorded into the frame command buffer
    // before present (while we still own the image), then read back after the fence.
    bool          mCapturePending       = false;  // armed for the next frame
    bool          mCaptureRecorded      = false;  // recordCapture ran this cycle
    glm::ivec2    mCaptureGameSize      = {};
    VkFence       mCaptureFence         = VK_NULL_HANDLE;
    VkImage       mCaptureImage         = VK_NULL_HANDLE;  // intermediate R8G8B8A8 (crop+convert+flip)
    VmaAllocation mCaptureImageAlloc    = VK_NULL_HANDLE;
    VkBuffer      mCaptureStaging       = VK_NULL_HANDLE;  // host-visible readback buffer
    VmaAllocation mCaptureStagingAlloc  = VK_NULL_HANDLE;
    void*         mCaptureStagingMapped = nullptr;
    VkDeviceSize  mCaptureBufSize       = 0;
};

using ActiveVulkanPresentation = WindowedVulkanPresentation;

#else // NOTHOFAGUS_HEADLESS_VULKAN

// ---------------------------------------------------------------------------
// HeadlessVulkanPresentation
// ---------------------------------------------------------------------------

/// Presentation policy for pure offscreen rendering — no surface, no swapchain.
/// Renders into a standard VkImage with TRANSFER_SRC_BIT for CPU readback.
struct HeadlessVulkanPresentation
{
    // --- Initialization (two-phase: target determines format, framebuffers need render pass) ---
    void createSurface(VkInstance instance, void* nativeWindowHandle);
    void configurePhysicalDeviceSelector(vkb::PhysicalDeviceSelector& selector);
    void retrieveQueues(vkb::Device& vkbDevice,
                        VkQueue& outGraphicsQueue, uint32_t& outGraphicsQueueFamily);
    /// presentMode is ignored — headless never presents. Accepted to match the
    /// windowed policy's interface (both are called identically from the backend).
    void createPresentationTarget(VkPhysicalDevice physicalDevice, VkDevice device,
                                  VmaAllocator allocator, VkFormat depthFormat,
                                  glm::ivec2 canvasSize, VkPresentModeKHR presentMode);
    void createPresentationFramebuffers(VkRenderPass mainRenderPass);
    void createSyncObjects(VkDevice device);

    // --- Render pass configuration ---
    VkFormat      colorFormat()        const;
    VkImageLayout mainPassFinalLayout() const;
    uint32_t      imageCount()         const;

    // --- Per-frame ---
    /// Acquire the next image. Headless has no swapchain, so framebuffer-size
    /// parameters are accepted (to satisfy the policy interface) and ignored.
    AcquireResult acquireImage(VkDevice device,
                               uint32_t framebufferWidth, uint32_t framebufferHeight);
    VkFramebuffer mainFramebuffer()    const;
    VkExtent2D    extent()             const;
    void          submitAndPresent(VkDevice device, VkQueue graphicsQueue,
                                   VkCommandBuffer commandBuffer, VkFence frameFence);

    // --- Screenshot (scheduled; headless reads its owned, never-presented image) ---
    void armCapture(glm::ivec2 gameSize);
    void recordCapture(VkCommandBuffer commandBuffer, ViewportRect gameViewport, VkFence frameFence);
    bool captureReady() const;
    ScreenshotPixels finishCapture(VkDevice device, VmaAllocator allocator,
                                   VkCommandPool commandPool, VkQueue graphicsQueue);

    // --- Cleanup ---
    void shutdown(VkDevice device, VmaAllocator allocator, VkInstance instance);

private:
    VkDevice      mDevice      = VK_NULL_HANDLE;
    VmaAllocator  mAllocator   = VK_NULL_HANDLE;
    VkFormat      mDepthFormat = VK_FORMAT_UNDEFINED;

    VkImage       mColorImage = VK_NULL_HANDLE;
    VmaAllocation mColorAlloc = VK_NULL_HANDLE;
    VkImageView   mColorView  = VK_NULL_HANDLE;

    VkImage       mDepthImage = VK_NULL_HANDLE;
    VmaAllocation mDepthAlloc = VK_NULL_HANDLE;
    VkImageView   mDepthView  = VK_NULL_HANDLE;

    VkFramebuffer mFramebuffer = VK_NULL_HANDLE;
    VkExtent2D    mExtent      = {};

    // Deferred screenshot state (headless has no present, so it reads its owned image).
    bool         mCapturePending  = false;
    bool         mCaptureRecorded = false;
    glm::ivec2   mCaptureGameSize = {};
    ViewportRect mCaptureViewport = {};
};

using ActiveVulkanPresentation = HeadlessVulkanPresentation;

#endif // NOTHOFAGUS_HEADLESS_VULKAN

} // namespace Nothofagus
