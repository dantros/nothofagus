#pragma once

#include <vulkan/vulkan.h>

// Forward-declare VMA handle to avoid including vk_mem_alloc.h in every TU.
struct VmaAllocation_T;
typedef VmaAllocation_T* VmaAllocation;

namespace Nothofagus
{

struct VulkanTexture
{
    VkImage         image         = VK_NULL_HANDLE;
    VmaAllocation   allocation    = VK_NULL_HANDLE;  // null for proxy textures
    VkImageView     imageView     = VK_NULL_HANDLE;
    VkSampler       sampler       = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    bool            isProxy       = false;  // owned by VulkanRenderTarget — skip vmaDestroyImage on free
    bool            isIndirect    = false;  // true for palette-based textures

    // Palette texture resources (only populated when isIndirect == true).
    VkImage         paletteImage      = VK_NULL_HANDLE;
    VmaAllocation   paletteAllocation = VK_NULL_HANDLE;
    VkImageView     paletteImageView  = VK_NULL_HANDLE;
    VkSampler       paletteSampler    = VK_NULL_HANDLE;
};

}
