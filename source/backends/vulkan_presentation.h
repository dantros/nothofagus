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

// Forward-declare GLFWwindow outside any namespace so it matches the GLFW header.
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
    // --- Initialization ---
    void createSurface(VkInstance instance, void* nativeWindowHandle);
    void configurePhysicalDeviceSelector(vkb::PhysicalDeviceSelector& selector);
    void retrieveQueues(vkb::Device& vkbDevice,
                        VkQueue& outGraphicsQueue, uint32_t& outGraphicsQueueFamily);
    void createPresentationTarget(VkPhysicalDevice physicalDevice, VkDevice device,
                                  VmaAllocator allocator, VkFormat depthFormat,
                                  VkRenderPass mainRenderPass, glm::ivec2 canvasSize);
    void createSyncObjects(VkDevice device);

    // --- Render pass configuration ---
    VkFormat      colorFormat()        const;
    VkImageLayout mainPassFinalLayout() const;
    uint32_t      imageCount()         const;

    // --- Per-frame ---
    AcquireResult acquireImage(VkDevice device);
    VkFramebuffer mainFramebuffer()    const;
    VkExtent2D    extent()             const;
    void          submitAndPresent(VkDevice device, VkQueue graphicsQueue,
                                   VkCommandBuffer commandBuffer, VkFence frameFence);

    // --- Screenshot ---
    ScreenshotPixels takeScreenshot(VkDevice device, VmaAllocator allocator,
                                    VkCommandPool commandPool, VkQueue graphicsQueue,
                                    ViewportRect gameViewport, glm::ivec2 gameSize) const;

    // --- Cleanup ---
    void shutdown(VkDevice device, VmaAllocator allocator, VkInstance instance);

private:
    void recreateSwapchain();
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

#if !defined(NOTHOFAGUS_BACKEND_SDL3)
    ::GLFWwindow* mGlfwWindow = nullptr;
#endif
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
    // --- Initialization ---
    void createSurface(VkInstance instance, void* nativeWindowHandle);
    void configurePhysicalDeviceSelector(vkb::PhysicalDeviceSelector& selector);
    void retrieveQueues(vkb::Device& vkbDevice,
                        VkQueue& outGraphicsQueue, uint32_t& outGraphicsQueueFamily);
    void createPresentationTarget(VkPhysicalDevice physicalDevice, VkDevice device,
                                  VmaAllocator allocator, VkFormat depthFormat,
                                  VkRenderPass mainRenderPass, glm::ivec2 canvasSize);
    void createSyncObjects(VkDevice device);

    // --- Render pass configuration ---
    VkFormat      colorFormat()        const;
    VkImageLayout mainPassFinalLayout() const;
    uint32_t      imageCount()         const;

    // --- Per-frame ---
    AcquireResult acquireImage(VkDevice device);
    VkFramebuffer mainFramebuffer()    const;
    VkExtent2D    extent()             const;
    void          submitAndPresent(VkDevice device, VkQueue graphicsQueue,
                                   VkCommandBuffer commandBuffer, VkFence frameFence);

    // --- Screenshot ---
    ScreenshotPixels takeScreenshot(VkDevice device, VmaAllocator allocator,
                                    VkCommandPool commandPool, VkQueue graphicsQueue,
                                    ViewportRect gameViewport, glm::ivec2 gameSize) const;

    // --- Cleanup ---
    void shutdown(VkDevice device, VmaAllocator allocator, VkInstance instance);

private:
    VkImage       mColorImage = VK_NULL_HANDLE;
    VmaAllocation mColorAlloc = VK_NULL_HANDLE;
    VkImageView   mColorView  = VK_NULL_HANDLE;

    VkImage       mDepthImage = VK_NULL_HANDLE;
    VmaAllocation mDepthAlloc = VK_NULL_HANDLE;
    VkImageView   mDepthView  = VK_NULL_HANDLE;

    VkFramebuffer mFramebuffer = VK_NULL_HANDLE;
    VkExtent2D    mExtent      = {};
};

using ActiveVulkanPresentation = HeadlessVulkanPresentation;

#endif // NOTHOFAGUS_HEADLESS_VULKAN

} // namespace Nothofagus
