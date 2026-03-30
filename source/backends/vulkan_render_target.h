#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <cstddef>

// Forward-declare VMA handle to avoid including vk_mem_alloc.h in every TU.
struct VmaAllocation_T;
typedef VmaAllocation_T* VmaAllocation;

namespace Nothofagus
{

struct VulkanRenderTarget
{
    // Color attachment — VK_FORMAT_R8G8B8A8_UNORM, COLOR_ATTACHMENT | SAMPLED
    VkImage       colorImage = VK_NULL_HANDLE;
    VmaAllocation colorAlloc = VK_NULL_HANDLE;
    VkImageView   colorView  = VK_NULL_HANDLE;

    // Depth attachment — VK_FORMAT_D24_UNORM_S8_UINT (fallback D32_SFLOAT)
    VkImage       depthImage = VK_NULL_HANDLE;
    VmaAllocation depthAlloc = VK_NULL_HANDLE;
    VkImageView   depthView  = VK_NULL_HANDLE;

    VkFramebuffer framebuffer   = VK_NULL_HANDLE;
    glm::ivec2    size          = {0, 0};
    std::size_t   proxyTextureId = 0;  // set by getRenderTargetTexture, consumed by freeRenderTarget
};

}
