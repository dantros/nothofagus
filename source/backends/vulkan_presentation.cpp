#include "vulkan_presentation.h"
#include <VkBootstrap.h>
#include <vk_mem_alloc.h>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <cstring>
#include <array>

#ifndef NOTHOFAGUS_HEADLESS_VULKAN
#  if defined(NOTHOFAGUS_BACKEND_SDL3)
#    include <SDL3/SDL.h>
#    include <SDL3/SDL_vulkan.h>
#  else
#    include <GLFW/glfw3.h>
#  endif
#endif

namespace Nothofagus
{

// ---------------------------------------------------------------------------
// Shared one-time command buffer helpers (used by both policies for screenshots)
// ---------------------------------------------------------------------------

static VkCommandBuffer beginOneTimeCommand(VkDevice device, VkCommandPool commandPool)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = commandPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

static void endOneTimeCommand(VkDevice device, VkCommandPool commandPool,
                              VkQueue graphicsQueue, VkCommandBuffer commandBuffer)
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

// ===========================================================================
#ifndef NOTHOFAGUS_HEADLESS_VULKAN
// ===========================================================================
// WindowedVulkanPresentation
// ===========================================================================

void WindowedVulkanPresentation::createSurface(VkInstance instance, void* nativeWindowHandle)
{
    mNativeWindowHandle = nativeWindowHandle;
#if defined(NOTHOFAGUS_BACKEND_SDL3)
    if (!SDL_Vulkan_CreateSurface(static_cast<SDL_Window*>(nativeWindowHandle), instance, nullptr, &mSurface))
        throw std::runtime_error("Failed to create Vulkan window surface (SDL3)");
#else
    if (glfwCreateWindowSurface(instance, static_cast<GLFWwindow*>(nativeWindowHandle), nullptr, &mSurface) != VK_SUCCESS)
        throw std::runtime_error("Failed to create Vulkan window surface (GLFW)");
#endif
}

void WindowedVulkanPresentation::configurePhysicalDeviceSelector(vkb::PhysicalDeviceSelector& selector)
{
    selector.set_surface(mSurface).require_present();
}

void WindowedVulkanPresentation::retrieveQueues(
    vkb::Device& vkbDevice, VkQueue& outGraphicsQueue, uint32_t& outGraphicsQueueFamily)
{
    outGraphicsQueue       = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    outGraphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
    mPresentQueue          = vkbDevice.get_queue(vkb::QueueType::present).value();
}

ScreenSize WindowedVulkanPresentation::queryFramebufferSize() const
{
    int width, height;
#if defined(NOTHOFAGUS_BACKEND_SDL3)
    SDL_GetWindowSizeInPixels(static_cast<SDL_Window*>(mNativeWindowHandle), &width, &height);
#else
    glfwGetFramebufferSize(static_cast<GLFWwindow*>(mNativeWindowHandle), &width, &height);
#endif
    return {static_cast<unsigned int>(width), static_cast<unsigned int>(height)};
}

void WindowedVulkanPresentation::createPresentationTarget(
    VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator allocator,
    VkFormat depthFormat, glm::ivec2 /*canvasSize*/)
{
    // Store handles for recreateSwapchain.
    mPhysicalDevice = physicalDevice;
    mDevice         = device;
    mAllocator      = allocator;
    mDepthFormat    = depthFormat;

    const ScreenSize framebufferSize = queryFramebufferSize();
    vkb::SwapchainBuilder swapchainBuilder{physicalDevice, device, mSurface};
    auto swapchainResult = swapchainBuilder
        .set_desired_format({VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
        .add_fallback_format({VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(framebufferSize.width, framebufferSize.height)
        .build();
    if (!swapchainResult)
        throw std::runtime_error("Failed to create swapchain: " + swapchainResult.error().message());

    vkb::Swapchain vkbSwapchain = swapchainResult.value();
    mSwapchain           = vkbSwapchain.swapchain;
    mSwapchainFormat     = vkbSwapchain.image_format;
    mSwapchainExtent     = vkbSwapchain.extent;
    mSwapchainImages     = vkbSwapchain.get_images().value();
    mSwapchainImageViews = vkbSwapchain.get_image_views().value();

    createDepthResources();
}

void WindowedVulkanPresentation::createPresentationFramebuffers(VkRenderPass mainRenderPass)
{
    mRenderPass = mainRenderPass;
    createFramebuffers();
}

void WindowedVulkanPresentation::createSyncObjects(VkDevice device)
{
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    const uint32_t imageCount = static_cast<uint32_t>(mSwapchainImages.size());
    mImageAvailableSemaphores.resize(imageCount);
    mRenderFinishedSemaphores.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; ++i)
    {
        if (vkCreateSemaphore(device, &semInfo, nullptr, &mImageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semInfo, nullptr, &mRenderFinishedSemaphores[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create swapchain semaphore");
    }
    mAcquireSemaphoreIndex = 0;
}

// --- Render pass configuration ---

VkFormat WindowedVulkanPresentation::colorFormat() const
{
    return mSwapchainFormat;
}

VkImageLayout WindowedVulkanPresentation::mainPassFinalLayout() const
{
    return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
}

uint32_t WindowedVulkanPresentation::imageCount() const
{
    return static_cast<uint32_t>(mSwapchainImages.size());
}

// --- Per-frame ---

AcquireResult WindowedVulkanPresentation::acquireImage(VkDevice device)
{
    VkSemaphore acquireSemaphore = mImageAvailableSemaphores[mAcquireSemaphoreIndex];
    VkResult acquireResult = vkAcquireNextImageKHR(
        device, mSwapchain, UINT64_MAX, acquireSemaphore, VK_NULL_HANDLE,
        &mCurrentImageIndex);

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        recreateSwapchain();
        return AcquireResult::Recreated;
    }

    return AcquireResult::Success;
}

VkFramebuffer WindowedVulkanPresentation::mainFramebuffer() const
{
    return mSwapchainFramebuffers[mCurrentImageIndex];
}

VkExtent2D WindowedVulkanPresentation::extent() const
{
    return mSwapchainExtent;
}

void WindowedVulkanPresentation::submitAndPresent(
    VkDevice /*device*/, VkQueue graphicsQueue, VkCommandBuffer commandBuffer, VkFence frameFence)
{
    VkSemaphore acquireSemaphore        = mImageAvailableSemaphores[mAcquireSemaphoreIndex];
    VkSemaphore renderFinishedSemaphore = mRenderFinishedSemaphores[mCurrentImageIndex];
    VkPipelineStageFlags waitStage      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo{};
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = &acquireSemaphore;
    submitInfo.pWaitDstStageMask    = &waitStage;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &renderFinishedSemaphore;
    vkQueueSubmit(graphicsQueue, 1, &submitInfo, frameFence);

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &renderFinishedSemaphore;
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &mSwapchain;
    presentInfo.pImageIndices      = &mCurrentImageIndex;

    VkResult presentResult = vkQueuePresentKHR(mPresentQueue, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR)
        recreateSwapchain();

    mAcquireSemaphoreIndex = (mAcquireSemaphoreIndex + 1) %
                             static_cast<uint32_t>(mImageAvailableSemaphores.size());
}

// --- Screenshot ---

ScreenshotPixels WindowedVulkanPresentation::takeScreenshot(
    VkDevice device, VmaAllocator allocator,
    VkCommandPool commandPool, VkQueue graphicsQueue,
    ViewportRect gameViewport, glm::ivec2 gameSize) const
{
    vkDeviceWaitIdle(device);

    const uint32_t     width  = static_cast<uint32_t>(gameSize.x);
    const uint32_t     height = static_cast<uint32_t>(gameSize.y);
    const VkDeviceSize bufSize = VkDeviceSize(width) * height * 4;

    // Staging buffer (host-visible, host-coherent)
    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size  = bufSize;
    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo stagingAllocInfo{};
    stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                             VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer stagingBuffer; VmaAllocation stagingAlloc; VmaAllocationInfo stagingMapped;
    vmaCreateBuffer(allocator, &bufInfo, &stagingAllocInfo, &stagingBuffer, &stagingAlloc, &stagingMapped);

    // Intermediate R8G8B8A8 image (handles B8G8R8A8 -> R8G8B8A8 format conversion)
    VkImageCreateInfo intermediateInfo{};
    intermediateInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    intermediateInfo.imageType     = VK_IMAGE_TYPE_2D;
    intermediateInfo.format        = VK_FORMAT_R8G8B8A8_UNORM;
    intermediateInfo.extent        = {width, height, 1};
    intermediateInfo.mipLevels     = 1;
    intermediateInfo.arrayLayers   = 1;
    intermediateInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    intermediateInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    intermediateInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    intermediateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo intermediateAllocInfo{};
    intermediateAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    VkImage intermediateImage; VmaAllocation intermediateAlloc;
    vmaCreateImage(allocator, &intermediateInfo, &intermediateAllocInfo,
                   &intermediateImage, &intermediateAlloc, nullptr);

    VkCommandBuffer commandBuffer = beginOneTimeCommand(device, commandPool);

    // Transition swapchain image to TRANSFER_SRC
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image               = mSwapchainImages[mCurrentImageIndex];
        barrier.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barrier.srcAccessMask       = VK_ACCESS_MEMORY_READ_BIT;
        barrier.dstAccessMask       = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    // Transition intermediate image to TRANSFER_DST
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image               = intermediateImage;
        barrier.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barrier.srcAccessMask       = 0;
        barrier.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    // Blit swapchain -> intermediate (viewport crop + format conversion)
    const int framebufferHeight = static_cast<int>(mSwapchainExtent.height);
    const int vulkanGameTop     = framebufferHeight - gameViewport.y - gameViewport.height;
    const int vulkanGameBottom  = framebufferHeight - gameViewport.y;

    VkImageBlit blit{};
    blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blit.srcOffsets[0]  = {gameViewport.x, vulkanGameTop, 0};
    blit.srcOffsets[1]  = {gameViewport.x + gameViewport.width, vulkanGameBottom, 1};
    blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blit.dstOffsets[0]  = {0, 0, 0};
    blit.dstOffsets[1]  = {static_cast<int32_t>(width), static_cast<int32_t>(height), 1};
    vkCmdBlitImage(commandBuffer,
        mSwapchainImages[mCurrentImageIndex], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        intermediateImage,                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &blit, VK_FILTER_NEAREST);

    // Transition intermediate to TRANSFER_SRC for copy to buffer
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image               = intermediateImage;
        barrier.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barrier.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask       = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    VkBufferImageCopy copyRegion{};
    copyRegion.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copyRegion.imageExtent      = {width, height, 1};
    vkCmdCopyImageToBuffer(commandBuffer, intermediateImage,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer, 1, &copyRegion);

    // Host-read barrier
    {
        VkBufferMemoryBarrier barrier{};
        barrier.sType         = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        barrier.buffer        = stagingBuffer;
        barrier.size          = bufSize;
        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
            0, 0, nullptr, 1, &barrier, 0, nullptr);
    }

    // Restore swapchain image to PRESENT_SRC
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image               = mSwapchainImages[mCurrentImageIndex];
        barrier.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barrier.srcAccessMask       = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask       = VK_ACCESS_MEMORY_READ_BIT;
        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    endOneTimeCommand(device, commandPool, graphicsQueue, commandBuffer);

    ScreenshotPixels result;
    result.width  = static_cast<int>(width);
    result.height = static_cast<int>(height);
    result.data.resize(bufSize);
    std::memcpy(result.data.data(), stagingMapped.pMappedData, bufSize);

    vmaDestroyBuffer(allocator, stagingBuffer, stagingAlloc);
    vmaDestroyImage(allocator, intermediateImage, intermediateAlloc);

    return result;
}

// --- Cleanup ---

void WindowedVulkanPresentation::shutdown(VkDevice device, VmaAllocator allocator, VkInstance instance)
{
    destroySwapchainResources();
    vkDestroySwapchainKHR(device, mSwapchain, nullptr);

    for (auto sem : mImageAvailableSemaphores)
        vkDestroySemaphore(device, sem, nullptr);
    mImageAvailableSemaphores.clear();
    for (auto sem : mRenderFinishedSemaphores)
        vkDestroySemaphore(device, sem, nullptr);
    mRenderFinishedSemaphores.clear();

    vkDestroySurfaceKHR(instance, mSurface, nullptr);
}

// --- Private helpers ---

void WindowedVulkanPresentation::createDepthResources()
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.format        = mDepthFormat;
    imageInfo.extent        = {mSwapchainExtent.width, mSwapchainExtent.height, 1};
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    if (vmaCreateImage(mAllocator, &imageInfo, &allocInfo,
                       &mDepthImage, &mDepthAlloc, nullptr) != VK_SUCCESS)
        throw std::runtime_error("Failed to create swapchain depth image");

    const bool hasStencil = (mDepthFormat == VK_FORMAT_D24_UNORM_S8_UINT);
    const VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT |
                                          (hasStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = mDepthImage;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = mDepthFormat;
    viewInfo.subresourceRange.aspectMask     = aspectMask;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.layerCount     = 1;

    if (vkCreateImageView(mDevice, &viewInfo, nullptr, &mDepthView) != VK_SUCCESS)
        throw std::runtime_error("Failed to create swapchain depth image view");
}

void WindowedVulkanPresentation::destroyDepthResources()
{
    if (mDepthView  != VK_NULL_HANDLE) vkDestroyImageView(mDevice, mDepthView, nullptr);
    if (mDepthImage != VK_NULL_HANDLE) vmaDestroyImage(mAllocator, mDepthImage, mDepthAlloc);
    mDepthView  = VK_NULL_HANDLE;
    mDepthImage = VK_NULL_HANDLE;
    mDepthAlloc = VK_NULL_HANDLE;
}

void WindowedVulkanPresentation::createFramebuffers()
{
    mSwapchainFramebuffers.resize(mSwapchainImageViews.size());
    for (std::size_t i = 0; i < mSwapchainImageViews.size(); ++i)
    {
        std::array<VkImageView, 2> attachments = {mSwapchainImageViews[i], mDepthView};

        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass      = mRenderPass;
        fbInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        fbInfo.pAttachments    = attachments.data();
        fbInfo.width           = mSwapchainExtent.width;
        fbInfo.height          = mSwapchainExtent.height;
        fbInfo.layers          = 1;

        if (vkCreateFramebuffer(mDevice, &fbInfo, nullptr, &mSwapchainFramebuffers[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create swapchain framebuffer");
    }
}

void WindowedVulkanPresentation::destroySwapchainResources()
{
    for (auto framebuffer : mSwapchainFramebuffers)
        vkDestroyFramebuffer(mDevice, framebuffer, nullptr);
    mSwapchainFramebuffers.clear();

    destroyDepthResources();

    for (auto imageView : mSwapchainImageViews)
        vkDestroyImageView(mDevice, imageView, nullptr);
    mSwapchainImageViews.clear();
}

void WindowedVulkanPresentation::recreateSwapchain()
{
    vkDeviceWaitIdle(mDevice);
    destroySwapchainResources();

    const ScreenSize framebufferSize = queryFramebufferSize();
    vkb::SwapchainBuilder builder{mPhysicalDevice, mDevice, mSurface};
    auto swapchainResult = builder
        .set_desired_format({VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
        .add_fallback_format({VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(framebufferSize.width, framebufferSize.height)
        .set_old_swapchain(mSwapchain)
        .build();
    if (!swapchainResult)
        throw std::runtime_error("Failed to recreate swapchain");

    vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);
    vkb::Swapchain vkbSwapchain = swapchainResult.value();
    mSwapchain           = vkbSwapchain.swapchain;
    mSwapchainFormat     = vkbSwapchain.image_format;
    mSwapchainExtent     = vkbSwapchain.extent;
    mSwapchainImages     = vkbSwapchain.get_images().value();
    mSwapchainImageViews = vkbSwapchain.get_image_views().value();

    createDepthResources();
    createFramebuffers();

    // Resize per-swapchain-image semaphores if the image count changed.
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    const uint32_t newImageCount = static_cast<uint32_t>(mSwapchainImages.size());

    auto resizeSemaphoreVector = [&](std::vector<VkSemaphore>& semaphores)
    {
        const uint32_t oldCount = static_cast<uint32_t>(semaphores.size());
        if (newImageCount > oldCount)
        {
            semaphores.resize(newImageCount);
            for (uint32_t i = oldCount; i < newImageCount; ++i)
                vkCreateSemaphore(mDevice, &semInfo, nullptr, &semaphores[i]);
        }
        else if (newImageCount < oldCount)
        {
            for (uint32_t i = newImageCount; i < oldCount; ++i)
                vkDestroySemaphore(mDevice, semaphores[i], nullptr);
            semaphores.resize(newImageCount);
        }
    };

    resizeSemaphoreVector(mImageAvailableSemaphores);
    resizeSemaphoreVector(mRenderFinishedSemaphores);
    mAcquireSemaphoreIndex = mAcquireSemaphoreIndex % newImageCount;
}

// ===========================================================================
#else // NOTHOFAGUS_HEADLESS_VULKAN
// ===========================================================================
// HeadlessVulkanPresentation
// ===========================================================================

void HeadlessVulkanPresentation::createSurface(VkInstance /*instance*/, void* /*nativeWindowHandle*/)
{
    // No surface needed for headless rendering.
}

void HeadlessVulkanPresentation::configurePhysicalDeviceSelector(vkb::PhysicalDeviceSelector& selector)
{
    // No surface or present capability required — just a graphics queue.
    selector.require_present(false).defer_surface_initialization();
}

void HeadlessVulkanPresentation::retrieveQueues(
    vkb::Device& vkbDevice, VkQueue& outGraphicsQueue, uint32_t& outGraphicsQueueFamily)
{
    outGraphicsQueue       = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    outGraphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
    // No present queue in headless mode.
}

void HeadlessVulkanPresentation::createPresentationTarget(
    VkPhysicalDevice /*physicalDevice*/, VkDevice device, VmaAllocator allocator,
    VkFormat depthFormat, glm::ivec2 canvasSize)
{
    mDevice      = device;
    mAllocator   = allocator;
    mDepthFormat = depthFormat;
    mExtent      = {static_cast<uint32_t>(canvasSize.x), static_cast<uint32_t>(canvasSize.y)};

    // --- Color image ---
    VkImageCreateInfo colorInfo{};
    colorInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    colorInfo.imageType     = VK_IMAGE_TYPE_2D;
    colorInfo.format        = VK_FORMAT_R8G8B8A8_UNORM;
    colorInfo.extent        = {mExtent.width, mExtent.height, 1};
    colorInfo.mipLevels     = 1;
    colorInfo.arrayLayers   = 1;
    colorInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    colorInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    colorInfo.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    colorInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo colorAllocInfo{};
    colorAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    if (vmaCreateImage(allocator, &colorInfo, &colorAllocInfo,
                       &mColorImage, &mColorAlloc, nullptr) != VK_SUCCESS)
        throw std::runtime_error("Failed to create offscreen color image");

    VkImageViewCreateInfo colorViewInfo{};
    colorViewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    colorViewInfo.image                           = mColorImage;
    colorViewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    colorViewInfo.format                          = VK_FORMAT_R8G8B8A8_UNORM;
    colorViewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    colorViewInfo.subresourceRange.levelCount     = 1;
    colorViewInfo.subresourceRange.layerCount     = 1;

    if (vkCreateImageView(device, &colorViewInfo, nullptr, &mColorView) != VK_SUCCESS)
        throw std::runtime_error("Failed to create offscreen color image view");

    // --- Depth image ---
    VkImageCreateInfo depthInfo{};
    depthInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depthInfo.imageType     = VK_IMAGE_TYPE_2D;
    depthInfo.format        = depthFormat;
    depthInfo.extent        = {mExtent.width, mExtent.height, 1};
    depthInfo.mipLevels     = 1;
    depthInfo.arrayLayers   = 1;
    depthInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    depthInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    depthInfo.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depthInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo depthAllocInfo{};
    depthAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    if (vmaCreateImage(allocator, &depthInfo, &depthAllocInfo,
                       &mDepthImage, &mDepthAlloc, nullptr) != VK_SUCCESS)
        throw std::runtime_error("Failed to create offscreen depth image");

    const bool hasStencil = (depthFormat == VK_FORMAT_D24_UNORM_S8_UINT);
    VkImageViewCreateInfo depthViewInfo{};
    depthViewInfo.sType                       = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depthViewInfo.image                       = mDepthImage;
    depthViewInfo.viewType                    = VK_IMAGE_VIEW_TYPE_2D;
    depthViewInfo.format                      = depthFormat;
    depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT |
                                                (hasStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u);
    depthViewInfo.subresourceRange.levelCount = 1;
    depthViewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &depthViewInfo, nullptr, &mDepthView) != VK_SUCCESS)
        throw std::runtime_error("Failed to create offscreen depth image view");
}

void HeadlessVulkanPresentation::createPresentationFramebuffers(VkRenderPass mainRenderPass)
{
    std::array<VkImageView, 2> attachments = {mColorView, mDepthView};
    VkFramebufferCreateInfo fbInfo{};
    fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass      = mainRenderPass;
    fbInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    fbInfo.pAttachments    = attachments.data();
    fbInfo.width           = mExtent.width;
    fbInfo.height          = mExtent.height;
    fbInfo.layers          = 1;

    if (vkCreateFramebuffer(mDevice, &fbInfo, nullptr, &mFramebuffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to create offscreen framebuffer");
}

void HeadlessVulkanPresentation::createSyncObjects(VkDevice /*device*/)
{
    // No semaphores needed — only fences (owned by VulkanBackend).
}

// --- Render pass configuration ---

VkFormat HeadlessVulkanPresentation::colorFormat() const
{
    return VK_FORMAT_R8G8B8A8_UNORM;
}

VkImageLayout HeadlessVulkanPresentation::mainPassFinalLayout() const
{
    return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
}

uint32_t HeadlessVulkanPresentation::imageCount() const
{
    return 2; // MAX_FRAMES_IN_FLIGHT
}

// --- Per-frame ---

AcquireResult HeadlessVulkanPresentation::acquireImage(VkDevice /*device*/)
{
    return AcquireResult::Success;
}

VkFramebuffer HeadlessVulkanPresentation::mainFramebuffer() const
{
    return mFramebuffer;
}

VkExtent2D HeadlessVulkanPresentation::extent() const
{
    return mExtent;
}

void HeadlessVulkanPresentation::submitAndPresent(
    VkDevice /*device*/, VkQueue graphicsQueue, VkCommandBuffer commandBuffer, VkFence frameFence)
{
    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &commandBuffer;
    vkQueueSubmit(graphicsQueue, 1, &submitInfo, frameFence);
    // No present — offscreen only.
}

// --- Screenshot ---

ScreenshotPixels HeadlessVulkanPresentation::takeScreenshot(
    VkDevice device, VmaAllocator allocator,
    VkCommandPool commandPool, VkQueue graphicsQueue,
    ViewportRect gameViewport, glm::ivec2 gameSize) const
{
    vkDeviceWaitIdle(device);

    const uint32_t     width  = static_cast<uint32_t>(gameSize.x);
    const uint32_t     height = static_cast<uint32_t>(gameSize.y);
    const VkDeviceSize bufSize = VkDeviceSize(width) * height * 4;

    // Staging buffer (host-visible, host-coherent)
    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size  = bufSize;
    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo stagingAllocInfo{};
    stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                             VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer stagingBuffer; VmaAllocation stagingAlloc; VmaAllocationInfo stagingMapped;
    vmaCreateBuffer(allocator, &bufInfo, &stagingAllocInfo, &stagingBuffer, &stagingAlloc, &stagingMapped);

    VkCommandBuffer commandBuffer = beginOneTimeCommand(device, commandPool);

    // Transition offscreen color image: COLOR_ATTACHMENT_OPTIMAL -> TRANSFER_SRC_OPTIMAL
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image               = mColorImage;
        barrier.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barrier.srcAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask       = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    // Copy the game viewport region from the offscreen image to the staging buffer.
    // gameViewport.y is in OpenGL convention (from bottom); convert to Vulkan (from top).
    const int framebufferHeight = static_cast<int>(mExtent.height);
    const int vulkanGameTop     = framebufferHeight - gameViewport.y - gameViewport.height;

    VkBufferImageCopy copyRegion{};
    copyRegion.bufferOffset      = 0;
    copyRegion.bufferRowLength   = 0; // tightly packed
    copyRegion.bufferImageHeight = 0;
    copyRegion.imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copyRegion.imageOffset       = {gameViewport.x, vulkanGameTop, 0};
    copyRegion.imageExtent       = {width, height, 1};
    vkCmdCopyImageToBuffer(commandBuffer, mColorImage,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer, 1, &copyRegion);

    // Host-read barrier
    {
        VkBufferMemoryBarrier barrier{};
        barrier.sType         = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        barrier.buffer        = stagingBuffer;
        barrier.size          = bufSize;
        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
            0, 0, nullptr, 1, &barrier, 0, nullptr);
    }

    // Restore offscreen image: TRANSFER_SRC_OPTIMAL -> COLOR_ATTACHMENT_OPTIMAL
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image               = mColorImage;
        barrier.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barrier.srcAccessMask       = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    endOneTimeCommand(device, commandPool, graphicsQueue, commandBuffer);

    ScreenshotPixels result;
    result.width  = static_cast<int>(width);
    result.height = static_cast<int>(height);
    result.data.resize(bufSize);
    std::memcpy(result.data.data(), stagingMapped.pMappedData, bufSize);

    vmaDestroyBuffer(allocator, stagingBuffer, stagingAlloc);

    return result;
}

// --- Cleanup ---

void HeadlessVulkanPresentation::shutdown(VkDevice device, VmaAllocator allocator, VkInstance /*instance*/)
{
    if (mFramebuffer != VK_NULL_HANDLE) vkDestroyFramebuffer(device, mFramebuffer, nullptr);
    if (mColorView   != VK_NULL_HANDLE) vkDestroyImageView(device, mColorView, nullptr);
    if (mColorImage  != VK_NULL_HANDLE) vmaDestroyImage(allocator, mColorImage, mColorAlloc);
    if (mDepthView   != VK_NULL_HANDLE) vkDestroyImageView(device, mDepthView, nullptr);
    if (mDepthImage  != VK_NULL_HANDLE) vmaDestroyImage(allocator, mDepthImage, mDepthAlloc);
}

#endif // NOTHOFAGUS_HEADLESS_VULKAN

} // namespace Nothofagus
