#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

// Forward-declare VMA handle to avoid including vk_mem_alloc.h in every TU.
struct VmaAllocation_T;
typedef VmaAllocation_T* VmaAllocation;

namespace Nothofagus
{

struct VulkanMesh
{
    VkBuffer      vertexBuffer = VK_NULL_HANDLE;
    VmaAllocation vertexAlloc  = VK_NULL_HANDLE;
    VkBuffer      indexBuffer  = VK_NULL_HANDLE;
    VmaAllocation indexAlloc   = VK_NULL_HANDLE;
    uint32_t      indexCount   = 0;
};

}
