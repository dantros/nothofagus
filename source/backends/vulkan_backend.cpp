#include "vulkan_backend.h"
#include <VkBootstrap.h>
#include <vk_mem_alloc.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <array>

namespace Nothofagus
{

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static SpritePushConstants packPushConstants(const SpriteDrawParams& params)
{
    SpritePushConstants pushConstants{};
    pushConstants.col0[0] = params.transform[0][0];
    pushConstants.col0[1] = params.transform[0][1];
    pushConstants.col0[2] = params.transform[0][2];
    pushConstants.col1[0] = params.transform[1][0];
    pushConstants.col1[1] = params.transform[1][1];
    pushConstants.col1[2] = params.transform[1][2];
    pushConstants.col2[0] = params.transform[2][0];
    pushConstants.col2[1] = params.transform[2][1];
    pushConstants.col2[2] = params.transform[2][2];
    pushConstants.layerIndex    = params.layerIndex;
    pushConstants.tintColor[0]  = params.tintColor.r;
    pushConstants.tintColor[1]  = params.tintColor.g;
    pushConstants.tintColor[2]  = params.tintColor.b;
    pushConstants.tintIntensity = params.tintIntensity;
    pushConstants.opacity       = params.opacity;
    return pushConstants;
}

// ---------------------------------------------------------------------------
// One-time command buffer helpers
// ---------------------------------------------------------------------------

VkCommandBuffer VulkanBackend::beginOneTimeCommandBuffer() const
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = mCommandPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(mDevice, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void VulkanBackend::endOneTimeCommandBuffer(VkCommandBuffer commandBuffer) const
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &commandBuffer;

    vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(mGraphicsQueue);

    vkFreeCommandBuffers(mDevice, mCommandPool, 1, &commandBuffer);
}

// ---------------------------------------------------------------------------
// Image layout transition helper
// ---------------------------------------------------------------------------

void VulkanBackend::transitionImageLayout(
    VkCommandBuffer commandBuffer, VkImage image,
    VkImageLayout oldLayout, VkImageLayout newLayout,
    VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
    VkAccessFlags srcAccess, VkAccessFlags dstAccess,
    uint32_t layerCount) const
{
    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout                       = oldLayout;
    barrier.newLayout                       = newLayout;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                           = image;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = layerCount;
    barrier.srcAccessMask                   = srcAccess;
    barrier.dstAccessMask                   = dstAccess;

    vkCmdPipelineBarrier(commandBuffer,
        srcStage, dstStage,
        0, 0, nullptr, 0, nullptr, 1, &barrier);
}

// ---------------------------------------------------------------------------
// Shader module
// ---------------------------------------------------------------------------

VkShaderModule VulkanBackend::createShaderModule(const char* spirvPath) const
{
    std::ifstream file(spirvPath, std::ios::ate | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error(std::string("Failed to open SPIR-V file: ") + spirvPath);

    const std::size_t fileSize = static_cast<std::size_t>(file.tellg());
    std::vector<char> code(fileSize);
    file.seekg(0);
    file.read(code.data(), static_cast<std::streamsize>(fileSize));

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(mDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shader module");
    return shaderModule;
}

// ---------------------------------------------------------------------------
// Sampler creation
// ---------------------------------------------------------------------------

VkSampler VulkanBackend::createSampler(TextureSampleMode minFilter, TextureSampleMode magFilter) const
{
    auto toVkFilter = [](TextureSampleMode mode) {
        return mode == TextureSampleMode::Linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    };

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.minFilter    = toVkFilter(minFilter);
    samplerInfo.magFilter    = toVkFilter(magFilter);
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.minLod       = 0.0f;
    samplerInfo.maxLod       = 0.0f;

    VkSampler sampler;
    if (vkCreateSampler(mDevice, &samplerInfo, nullptr, &sampler) != VK_SUCCESS)
        throw std::runtime_error("Failed to create sampler");
    return sampler;
}

// ---------------------------------------------------------------------------
// Descriptor set allocation + update
// ---------------------------------------------------------------------------

VkDescriptorSet VulkanBackend::allocateAndUpdateDescriptorSet(
    VkImageView imageView, VkSampler sampler) const
{
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = mDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &mDescriptorSetLayout;

    VkDescriptorSet descriptorSet;
    if (vkAllocateDescriptorSets(mDevice, &allocInfo, &descriptorSet) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate descriptor set");

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView   = imageView;
    imageInfo.sampler     = sampler;

    VkWriteDescriptorSet write{};
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet          = descriptorSet;
    write.dstBinding      = 0;
    write.descriptorCount = 1;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo      = &imageInfo;
    vkUpdateDescriptorSets(mDevice, 1, &write, 0, nullptr);

    return descriptorSet;
}

// ---------------------------------------------------------------------------
// Depth format selection
// ---------------------------------------------------------------------------

VkFormat VulkanBackend::findDepthFormat() const
{
    for (VkFormat format : {VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT})
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(mPhysicalDevice, format, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            return format;
    }
    throw std::runtime_error("Failed to find supported depth format");
}

// ---------------------------------------------------------------------------
// initialize()
// ---------------------------------------------------------------------------

void VulkanBackend::initialize(void* nativeWindowHandle, glm::ivec2 canvasSize)
{
    // 1. Instance
    // Disable Samsung Galaxy overlay implicit layers before the Vulkan loader
    // enumerates them. They don't follow the VK_LAYER_ naming convention and the
    // loader prints a direct warning to stderr for each one — those prints happen
    // before any debug-messenger callback is active, so they can't be filtered
    // there. VK_LOADER_LAYERS_DISABLE glob support requires loader 1.3.234+.
#if defined(_WIN32)
    _putenv_s("VK_LOADER_LAYERS_DISABLE", "*GalaxyOverlay*");
#else
    setenv("VK_LOADER_LAYERS_DISABLE", "*GalaxyOverlay*", 1);
#endif
    vkb::InstanceBuilder instanceBuilder;
    instanceBuilder.set_app_name("Nothofagus").require_api_version(1, 1, 0);
#ifndef NDEBUG
    instanceBuilder.enable_validation_layers().use_default_debug_messenger();
#endif
    auto instanceResult = instanceBuilder.build();
    if (!instanceResult)
        throw std::runtime_error("Failed to create Vulkan instance: " + instanceResult.error().message());

    vkb::Instance vkbInstance = instanceResult.value();
    mInstance       = vkbInstance.instance;
    mDebugMessenger = vkbInstance.debug_messenger;

    // 2. Surface (delegated to presentation policy — no-op in headless mode)
    mPresentation.createSurface(mInstance, nativeWindowHandle);

    // 3. Physical device
    vkb::PhysicalDeviceSelector physSelector{vkbInstance};
    mPresentation.configurePhysicalDeviceSelector(physSelector);
    auto physResult = physSelector
        .set_minimum_version(1, 1)
        .select();
    if (!physResult)
        throw std::runtime_error("Failed to select physical device: " + physResult.error().message());

    vkb::PhysicalDevice vkbPhysical = physResult.value();
    mPhysicalDevice = vkbPhysical.physical_device;

    // 4. Logical device + queues
    vkb::DeviceBuilder deviceBuilder{vkbPhysical};
    auto deviceResult = deviceBuilder.build();
    if (!deviceResult)
        throw std::runtime_error("Failed to create logical device: " + deviceResult.error().message());

    vkb::Device vkbDevice = deviceResult.value();
    mDevice = vkbDevice.device;
    mPresentation.retrieveQueues(vkbDevice, mGraphicsQueue, mGraphicsQueueFamily);

    // 5. VMA
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.physicalDevice   = mPhysicalDevice;
    allocatorInfo.device           = mDevice;
    allocatorInfo.instance         = mInstance;
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_1;
    if (vmaCreateAllocator(&allocatorInfo, &mAllocator) != VK_SUCCESS)
        throw std::runtime_error("Failed to create VMA allocator");

    // 6. Command pool
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = mGraphicsQueueFamily;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(mDevice, &poolInfo, nullptr, &mCommandPool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create command pool");

    // 7. Per-frame command buffers + fences
    VkCommandBufferAllocateInfo cbAllocInfo{};
    cbAllocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbAllocInfo.commandPool        = mCommandPool;
    cbAllocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbAllocInfo.commandBufferCount = 1;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (auto& frame : mFrames)
    {
        if (vkAllocateCommandBuffers(mDevice, &cbAllocInfo, &frame.commandBuffer) != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate command buffer");
        if (vkCreateFence(mDevice, &fenceInfo, nullptr, &frame.inFlight) != VK_SUCCESS)
            throw std::runtime_error("Failed to create frame sync primitives");
    }

    // 8. Depth format
    mDepthFormat = findDepthFormat();

    // 9. Presentation target (swapchain or offscreen image — determines color format)
    mPresentation.createPresentationTarget(mPhysicalDevice, mDevice, mAllocator,
                                           mDepthFormat, canvasSize);

    // 10. Main render pass (format and final layout come from the presentation policy)
    {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format         = mPresentation.colorFormat();
        colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout    = mPresentation.mainPassFinalLayout();

        VkAttachmentDescription depthAttachment{};
        depthAttachment.format         = mDepthFormat;
        depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount    = 1;
        subpass.pColorAttachments       = &colorRef;
        subpass.pDepthStencilAttachment = &depthRef;

        VkSubpassDependency dep{};
        dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass    = 0;
        dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dep.srcAccessMask = 0;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
        VkRenderPassCreateInfo rpInfo{};
        rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        rpInfo.pAttachments    = attachments.data();
        rpInfo.subpassCount    = 1;
        rpInfo.pSubpasses      = &subpass;
        rpInfo.dependencyCount = 1;
        rpInfo.pDependencies   = &dep;

        if (vkCreateRenderPass(mDevice, &rpInfo, nullptr, &mMainRenderPass) != VK_SUCCESS)
            throw std::runtime_error("Failed to create main render pass");
    }

    // 11. RTT render pass (off-screen R8G8B8A8, stays COLOR_ATTACHMENT_OPTIMAL)
    {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format         = VK_FORMAT_R8G8B8A8_UNORM;
        colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription depthAttachment{};
        depthAttachment.format         = mDepthFormat;
        depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount    = 1;
        subpass.pColorAttachments       = &colorRef;
        subpass.pDepthStencilAttachment = &depthRef;

        VkSubpassDependency dep{};
        dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass    = 0;
        dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dep.srcAccessMask = 0;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
        VkRenderPassCreateInfo rpInfo{};
        rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        rpInfo.pAttachments    = attachments.data();
        rpInfo.subpassCount    = 1;
        rpInfo.pSubpasses      = &subpass;
        rpInfo.dependencyCount = 1;
        rpInfo.pDependencies   = &dep;

        if (vkCreateRenderPass(mDevice, &rpInfo, nullptr, &mRttRenderPass) != VK_SUCCESS)
            throw std::runtime_error("Failed to create RTT render pass");
    }

    // 12. Presentation framebuffers (need the render pass created above)
    mPresentation.createPresentationFramebuffers(mMainRenderPass);
    mPresentation.createSyncObjects(mDevice);

    // 13. Descriptor set layout (binding 0 = combined image sampler, fragment stage)
    {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding         = 0;
        binding.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = 1;
        binding.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings    = &binding;

        if (vkCreateDescriptorSetLayout(mDevice, &layoutInfo, nullptr, &mDescriptorSetLayout) != VK_SUCCESS)
            throw std::runtime_error("Failed to create descriptor set layout");
    }

    // 14. Descriptor pools
    {
        VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024};
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets       = 1024;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes    = &poolSize;
        if (vkCreateDescriptorPool(mDevice, &poolInfo, nullptr, &mDescriptorPool) != VK_SUCCESS)
            throw std::runtime_error("Failed to create descriptor pool");
    }
    {
        VkDescriptorPoolSize imguiPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100};
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets       = 100;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes    = &imguiPoolSize;
        if (vkCreateDescriptorPool(mDevice, &poolInfo, nullptr, &mImguiDescriptorPool) != VK_SUCCESS)
            throw std::runtime_error("Failed to create ImGui descriptor pool");
    }

    // 15. Pipeline layout
    {
        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushRange.offset     = 0;
        pushRange.size       = sizeof(SpritePushConstants);

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount         = 1;
        layoutInfo.pSetLayouts            = &mDescriptorSetLayout;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges    = &pushRange;
        if (vkCreatePipelineLayout(mDevice, &layoutInfo, nullptr, &mPipelineLayout) != VK_SUCCESS)
            throw std::runtime_error("Failed to create pipeline layout");
    }

    // 16. Graphics pipeline
    {
        VkShaderModule vertModule = createShaderModule(NOTHOFAGUS_SPRITE_VERT_SPV);
        VkShaderModule fragModule = createShaderModule(NOTHOFAGUS_SPRITE_FRAG_SPV);

        std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
        stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertModule;
        stages[0].pName  = "main";
        stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragModule;
        stages[1].pName  = "main";

        // Vertex layout: binding 0, stride 16 bytes — vec2 position (offset 0), vec2 uv (offset 8)
        VkVertexInputBindingDescription bindingDesc{};
        bindingDesc.binding   = 0;
        bindingDesc.stride    = sizeof(float) * 4;
        bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::array<VkVertexInputAttributeDescription, 2> attrDescs{};
        attrDescs[0] = {0, 0, VK_FORMAT_R32G32_SFLOAT, 0};
        attrDescs[1] = {1, 0, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 2};

        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount   = 1;
        vertexInput.pVertexBindingDescriptions      = &bindingDesc;
        vertexInput.vertexAttributeDescriptionCount = 2;
        vertexInput.pVertexAttributeDescriptions    = attrDescs.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount  = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.cullMode    = VK_CULL_MODE_NONE;
        rasterizer.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.lineWidth   = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable  = VK_FALSE;
        depthStencil.depthWriteEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState blendAttachment{};
        blendAttachment.blendEnable         = VK_TRUE;
        blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
        blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;
        blendAttachment.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments    = &blendAttachment;

        std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates    = dynamicStates.data();

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount          = static_cast<uint32_t>(stages.size());
        pipelineInfo.pStages             = stages.data();
        pipelineInfo.pVertexInputState   = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState      = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState   = &multisampling;
        pipelineInfo.pDepthStencilState  = &depthStencil;
        pipelineInfo.pColorBlendState    = &colorBlending;
        pipelineInfo.pDynamicState       = &dynamicState;
        pipelineInfo.layout              = mPipelineLayout;
        pipelineInfo.renderPass          = mMainRenderPass;
        pipelineInfo.subpass             = 0;

        if (vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mSpritePipeline) != VK_SUCCESS)
            throw std::runtime_error("Failed to create graphics pipeline");

        vkDestroyShaderModule(mDevice, fragModule, nullptr);

        // 17. Indirect (palette-based) descriptor set layout — 2 bindings
        {
            std::array<VkDescriptorSetLayoutBinding, 2> indirectBindings{};
            // binding 0: index texture (usampler2DArray)
            indirectBindings[0].binding         = 0;
            indirectBindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            indirectBindings[0].descriptorCount = 1;
            indirectBindings[0].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
            // binding 1: palette texture (sampler1D)
            indirectBindings[1].binding         = 1;
            indirectBindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            indirectBindings[1].descriptorCount = 1;
            indirectBindings[1].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

            VkDescriptorSetLayoutCreateInfo indirectLayoutInfo{};
            indirectLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            indirectLayoutInfo.bindingCount = static_cast<uint32_t>(indirectBindings.size());
            indirectLayoutInfo.pBindings    = indirectBindings.data();

            if (vkCreateDescriptorSetLayout(mDevice, &indirectLayoutInfo, nullptr, &mIndirectDescriptorSetLayout) != VK_SUCCESS)
                throw std::runtime_error("Failed to create indirect descriptor set layout");
        }

        // 18. Indirect pipeline layout (same push constants, different descriptor set layout)
        {
            VkPushConstantRange indirectPushRange{};
            indirectPushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
            indirectPushRange.offset     = 0;
            indirectPushRange.size       = sizeof(SpritePushConstants);

            VkPipelineLayoutCreateInfo indirectPipelineLayoutInfo{};
            indirectPipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            indirectPipelineLayoutInfo.setLayoutCount         = 1;
            indirectPipelineLayoutInfo.pSetLayouts            = &mIndirectDescriptorSetLayout;
            indirectPipelineLayoutInfo.pushConstantRangeCount = 1;
            indirectPipelineLayoutInfo.pPushConstantRanges    = &indirectPushRange;
            if (vkCreatePipelineLayout(mDevice, &indirectPipelineLayoutInfo, nullptr, &mIndirectPipelineLayout) != VK_SUCCESS)
                throw std::runtime_error("Failed to create indirect pipeline layout");
        }

        // 19. Indirect graphics pipeline (same vertex shader, indirect fragment shader)
        {
            VkShaderModule indirectFragModule = createShaderModule(NOTHOFAGUS_SPRITE_INDIRECT_FRAG_SPV);

            stages[1].module = indirectFragModule;

            pipelineInfo.layout = mIndirectPipelineLayout;

            if (vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mIndirectSpritePipeline) != VK_SUCCESS)
                throw std::runtime_error("Failed to create indirect graphics pipeline");

            vkDestroyShaderModule(mDevice, indirectFragModule, nullptr);
        }

        vkDestroyShaderModule(mDevice, vertModule, nullptr);
    }

}

// ---------------------------------------------------------------------------
// initImGuiRenderer()
// ---------------------------------------------------------------------------

void VulkanBackend::initImGuiRenderer()
{
    ImGui_ImplVulkan_InitInfo imguiInfo{};
    imguiInfo.Instance        = mInstance;
    imguiInfo.PhysicalDevice  = mPhysicalDevice;
    imguiInfo.Device          = mDevice;
    imguiInfo.QueueFamily     = mGraphicsQueueFamily;
    imguiInfo.Queue           = mGraphicsQueue;
    imguiInfo.DescriptorPool  = mImguiDescriptorPool;
    imguiInfo.RenderPass      = mMainRenderPass;
    imguiInfo.MinImageCount   = 2;
    imguiInfo.ImageCount      = mPresentation.imageCount();
    imguiInfo.MSAASamples     = VK_SAMPLE_COUNT_1_BIT;
    ImGui_ImplVulkan_Init(&imguiInfo);
    // Font upload is deferred: ImGui_ImplVulkan_NewFrame() calls
    // ImGui_ImplVulkan_CreateFontsTexture() automatically on the first frame,
    // after canvas_impl has finished adding custom fonts (Roboto, etc.).
}

// ---------------------------------------------------------------------------
// flushPendingDeletions()
// ---------------------------------------------------------------------------

void VulkanBackend::flushPendingDeletions(FrameData& frame)
{
    for (auto& pending : frame.pendingBufferDeletions)
        vmaDestroyBuffer(mAllocator, pending.buffer, pending.allocation);
    frame.pendingBufferDeletions.clear();

    for (auto& pending : frame.pendingTextureDeletions)
    {
        vkFreeDescriptorSets(mDevice, mDescriptorPool, 1, &pending.descriptorSet);
        vkDestroySampler(mDevice, pending.sampler, nullptr);
        vkDestroyImageView(mDevice, pending.imageView, nullptr);
        if (pending.image != VK_NULL_HANDLE)
            vmaDestroyImage(mAllocator, pending.image, pending.allocation);
        // Palette resources (indirect textures only).
        if (pending.paletteSampler != VK_NULL_HANDLE)
            vkDestroySampler(mDevice, pending.paletteSampler, nullptr);
        if (pending.paletteImageView != VK_NULL_HANDLE)
            vkDestroyImageView(mDevice, pending.paletteImageView, nullptr);
        if (pending.paletteImage != VK_NULL_HANDLE)
            vmaDestroyImage(mAllocator, pending.paletteImage, pending.paletteAllocation);
    }
    frame.pendingTextureDeletions.clear();

    for (auto& pending : frame.pendingRenderTargetDeletions)
    {
        vkFreeDescriptorSets(mDevice, mDescriptorPool, 1, &pending.proxyDescriptorSet);
        vkDestroySampler(mDevice, pending.proxySampler, nullptr);
        vkDestroyFramebuffer(mDevice, pending.framebuffer, nullptr);
        vkDestroyImageView(mDevice, pending.colorView, nullptr);
        vkDestroyImageView(mDevice, pending.depthView, nullptr);
        vmaDestroyImage(mAllocator, pending.colorImage, pending.colorAlloc);
        vmaDestroyImage(mAllocator, pending.depthImage, pending.depthAlloc);
    }
    frame.pendingRenderTargetDeletions.clear();
}

// ---------------------------------------------------------------------------
// shutdown()
// ---------------------------------------------------------------------------

void VulkanBackend::shutdown()
{
    vkDeviceWaitIdle(mDevice);

    ImGui_ImplVulkan_Shutdown();

    // Flush deferred deletions from all frame slots — these were queued by
    // freeTexture/freeMesh/freeRenderTarget during the last frames and never
    // flushed because beginFrame() wasn't called again before shutdown.
    for (auto& frame : mFrames)
        flushPendingDeletions(frame);

    // Free all GPU resources
    for (auto& [id, mesh] : mMeshes)
    {
        vmaDestroyBuffer(mAllocator, mesh.vertexBuffer, mesh.vertexAlloc);
        vmaDestroyBuffer(mAllocator, mesh.indexBuffer,  mesh.indexAlloc);
    }
    mMeshes.clear();

    for (auto& [id, tex] : mTextures)
    {
        vkFreeDescriptorSets(mDevice, mDescriptorPool, 1, &tex.descriptorSet);
        vkDestroySampler(mDevice, tex.sampler, nullptr);
        vkDestroyImageView(mDevice, tex.imageView, nullptr);
        if (!tex.isProxy)
            vmaDestroyImage(mAllocator, tex.image, tex.allocation);
        if (tex.isIndirect)
        {
            vkDestroySampler(mDevice, tex.paletteSampler, nullptr);
            vkDestroyImageView(mDevice, tex.paletteImageView, nullptr);
            vmaDestroyImage(mAllocator, tex.paletteImage, tex.paletteAllocation);
        }
    }
    mTextures.clear();

    for (auto& [id, rt] : mRenderTargets)
    {
        vkDestroyFramebuffer(mDevice, rt.framebuffer, nullptr);
        vkDestroyImageView(mDevice, rt.colorView, nullptr);
        vkDestroyImageView(mDevice, rt.depthView, nullptr);
        vmaDestroyImage(mAllocator, rt.colorImage, rt.colorAlloc);
        vmaDestroyImage(mAllocator, rt.depthImage, rt.depthAlloc);
    }
    mRenderTargets.clear();

    vkDestroyPipeline(mDevice, mIndirectSpritePipeline, nullptr);
    vkDestroyPipelineLayout(mDevice, mIndirectPipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(mDevice, mIndirectDescriptorSetLayout, nullptr);
    vkDestroyPipeline(mDevice, mSpritePipeline, nullptr);
    vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(mDevice, mDescriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);
    vkDestroyDescriptorPool(mDevice, mImguiDescriptorPool, nullptr);

    vkDestroyRenderPass(mDevice, mRttRenderPass, nullptr);
    vkDestroyRenderPass(mDevice, mMainRenderPass, nullptr);

    mPresentation.shutdown(mDevice, mAllocator, mInstance);

    for (auto& frame : mFrames)
        vkDestroyFence(mDevice, frame.inFlight, nullptr);

    vkDestroyCommandPool(mDevice, mCommandPool, nullptr);
    vmaDestroyAllocator(mAllocator);
    vkDestroyDevice(mDevice, nullptr);

    if (mDebugMessenger != VK_NULL_HANDLE)
    {
        auto destroyFn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(mInstance, "vkDestroyDebugUtilsMessengerEXT"));
        if (destroyFn)
            destroyFn(mInstance, mDebugMessenger, nullptr);
    }

    vkDestroyInstance(mInstance, nullptr);
}

// ---------------------------------------------------------------------------
// uploadTexture()
// ---------------------------------------------------------------------------

DTexture VulkanBackend::uploadTexture(
    const Texture& texture, TextureSampleMode minFilter, TextureSampleMode magFilter)
{
    const bool isIndirect = std::holds_alternative<IndirectTexture>(texture);

    // Determine upload data and format based on texture type.
    VkFormat imageFormat;
    const void* uploadData;
    VkDeviceSize imageSize;
    uint32_t width, height, layers;
    std::vector<std::uint8_t> indexData;  // kept alive until after staging copy
    TextureData directTextureData(1, 1);  // kept alive until after staging copy

    if (isIndirect)
    {
        const auto& indirectTexture = std::get<IndirectTexture>(texture);
        indexData  = indirectTexture.generateIndexData();
        width      = static_cast<uint32_t>(indirectTexture.size().x);
        height     = static_cast<uint32_t>(indirectTexture.size().y);
        layers     = static_cast<uint32_t>(indirectTexture.layers());
        imageSize  = indexData.size();
        uploadData = indexData.data();
        imageFormat = VK_FORMAT_R8_UINT;
    }
    else
    {
        directTextureData = std::visit(GenerateTextureDataVisitor{}, texture);
        width      = static_cast<uint32_t>(directTextureData.width());
        height     = static_cast<uint32_t>(directTextureData.height());
        layers     = static_cast<uint32_t>(directTextureData.layers());
        imageSize  = directTextureData.getDataSpan().size();
        uploadData = directTextureData.getDataSpan().data();
        imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
    }

    // Staging buffer
    VkBufferCreateInfo stagingBufferInfo{};
    stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingBufferInfo.size  = imageSize;
    stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo stagingAllocInfo{};
    stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                             VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer      stagingBuffer;
    VmaAllocation stagingAlloc;
    VmaAllocationInfo stagingInfo;
    vmaCreateBuffer(mAllocator, &stagingBufferInfo, &stagingAllocInfo,
                    &stagingBuffer, &stagingAlloc, &stagingInfo);

    std::memcpy(stagingInfo.pMappedData, uploadData, imageSize);

    // Device-local image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.format        = imageFormat;
    imageInfo.extent        = {width, height, 1};
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = layers;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo imageAllocInfo{};
    imageAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    VkImage       image;
    VmaAllocation imageAlloc;
    vmaCreateImage(mAllocator, &imageInfo, &imageAllocInfo, &image, &imageAlloc, nullptr);

    // Upload via one-time command buffer
    VkCommandBuffer commandBuffer = beginOneTimeCommandBuffer();

    transitionImageLayout(commandBuffer, image,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, VK_ACCESS_TRANSFER_WRITE_BIT, layers);

    VkBufferImageCopy region{};
    region.bufferOffset      = 0;
    region.bufferRowLength   = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, layers};
    region.imageExtent       = {width, height, 1};
    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    transitionImageLayout(commandBuffer, image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, layers);

    endOneTimeCommandBuffer(commandBuffer);
    vmaDestroyBuffer(mAllocator, stagingBuffer, stagingAlloc);

    // Image view (2D array)
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = image;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    viewInfo.format                          = imageFormat;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = layers;

    VkImageView imageView;
    if (vkCreateImageView(mDevice, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
        throw std::runtime_error("Failed to create texture image view");

    // For indirect textures, force NEAREST filter (integer textures require it).
    const TextureSampleMode effectiveMinFilter = isIndirect ? TextureSampleMode::Nearest : minFilter;
    const TextureSampleMode effectiveMagFilter = isIndirect ? TextureSampleMode::Nearest : magFilter;
    VkSampler sampler = createSampler(effectiveMinFilter, effectiveMagFilter);

    VulkanTexture vulkanTexture{};
    vulkanTexture.image       = image;
    vulkanTexture.allocation  = imageAlloc;
    vulkanTexture.imageView   = imageView;
    vulkanTexture.sampler     = sampler;
    vulkanTexture.isProxy     = false;
    vulkanTexture.isIndirect  = isIndirect;

    if (isIndirect)
    {
        // For indirect textures, the descriptor set uses the indirect layout
        // and will be created when uploadPaletteTexture provides the palette image.
        // For now, create a placeholder single-binding descriptor set so drawSprite
        // doesn't crash if called before the palette is uploaded.
        vulkanTexture.descriptorSet = allocateAndUpdateDescriptorSet(imageView, sampler);
    }
    else
    {
        vulkanTexture.descriptorSet = allocateAndUpdateDescriptorSet(imageView, sampler);
    }

    const std::size_t newId = mNextId++;
    mTextures[newId] = vulkanTexture;
    return DTexture{newId};
}

// ---------------------------------------------------------------------------
// freeTexture()
// ---------------------------------------------------------------------------

void VulkanBackend::freeTexture(DTexture dtexture)
{
    auto it = mTextures.find(dtexture.id);
    if (it == mTextures.end()) return;

    VulkanTexture& tex = it->second;
    const int lastSubmittedSlot = (mCurrentFrame - 1 + MAX_FRAMES_IN_FLIGHT) % MAX_FRAMES_IN_FLIGHT;
    PendingTextureDeletion pending{};
    pending.descriptorSet = tex.descriptorSet;
    pending.sampler       = tex.sampler;
    pending.imageView     = tex.imageView;
    pending.image         = tex.isProxy ? VK_NULL_HANDLE : tex.image;
    pending.allocation    = tex.isProxy ? nullptr         : tex.allocation;
    if (tex.isIndirect)
    {
        pending.paletteSampler    = tex.paletteSampler;
        pending.paletteImageView  = tex.paletteImageView;
        pending.paletteImage      = tex.paletteImage;
        pending.paletteAllocation = tex.paletteAllocation;
    }
    mFrames[lastSubmittedSlot].pendingTextureDeletions.push_back(pending);
    mTextures.erase(it);
}

// ---------------------------------------------------------------------------
// Palette texture upload (indirect textures)
// ---------------------------------------------------------------------------

DTexture VulkanBackend::uploadPaletteTexture(const std::vector<glm::vec4>& paletteColors)
{
    // This is a stub — for Vulkan, palette data is stored inside the VulkanTexture
    // alongside the index texture. The actual upload happens in uploadTexture() via
    // the canvas integration calling this after uploadTexture. We create a 1D image
    // and store it with a dummy DTexture so the canvas can track it.

    const uint32_t paletteSize = static_cast<uint32_t>(paletteColors.size());
    const VkDeviceSize dataSize = paletteSize * sizeof(glm::vec4);

    // Staging buffer
    VkBufferCreateInfo stagingBufferInfo{};
    stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingBufferInfo.size  = dataSize;
    stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo stagingAllocInfo{};
    stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                             VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer      stagingBuffer;
    VmaAllocation stagingAlloc;
    VmaAllocationInfo stagingInfo;
    vmaCreateBuffer(mAllocator, &stagingBufferInfo, &stagingAllocInfo,
                    &stagingBuffer, &stagingAlloc, &stagingInfo);
    std::memcpy(stagingInfo.pMappedData, paletteColors.data(), dataSize);

    // 1D device-local image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_1D;
    imageInfo.format        = VK_FORMAT_R32G32B32A32_SFLOAT;
    imageInfo.extent        = {paletteSize, 1, 1};
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo imageAllocInfo{};
    imageAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    VkImage       paletteImage;
    VmaAllocation paletteAlloc;
    vmaCreateImage(mAllocator, &imageInfo, &imageAllocInfo, &paletteImage, &paletteAlloc, nullptr);

    VkCommandBuffer commandBuffer = beginOneTimeCommandBuffer();

    transitionImageLayout(commandBuffer, paletteImage,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, VK_ACCESS_TRANSFER_WRITE_BIT);

    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent      = {paletteSize, 1, 1};
    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, paletteImage,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    transitionImageLayout(commandBuffer, paletteImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);

    endOneTimeCommandBuffer(commandBuffer);
    vmaDestroyBuffer(mAllocator, stagingBuffer, stagingAlloc);

    // 1D image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = paletteImage;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_1D;
    viewInfo.format                          = VK_FORMAT_R32G32B32A32_SFLOAT;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.layerCount     = 1;

    VkImageView paletteImageView;
    if (vkCreateImageView(mDevice, &viewInfo, nullptr, &paletteImageView) != VK_SUCCESS)
        throw std::runtime_error("Failed to create palette image view");

    VkSampler paletteSampler = createSampler(TextureSampleMode::Nearest, TextureSampleMode::Nearest);

    // Store as a regular texture entry so DTexture handle works.
    std::size_t newId = mNextId++;
    VulkanTexture paletteTex{};
    paletteTex.image         = paletteImage;
    paletteTex.allocation    = paletteAlloc;
    paletteTex.imageView     = paletteImageView;
    paletteTex.sampler       = paletteSampler;
    paletteTex.descriptorSet = allocateAndUpdateDescriptorSet(paletteImageView, paletteSampler);
    mTextures[newId] = paletteTex;
    return DTexture{newId};
}

void VulkanBackend::updatePaletteTexture(DTexture paletteTexture,
                                          const std::vector<glm::vec4>& paletteColors)
{
    auto it = mTextures.find(paletteTexture.id);
    if (it == mTextures.end()) return;

    VulkanTexture& tex = it->second;
    const uint32_t paletteSize = static_cast<uint32_t>(paletteColors.size());
    const VkDeviceSize dataSize = paletteSize * sizeof(glm::vec4);

    // Staging buffer
    VkBufferCreateInfo stagingBufferInfo{};
    stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingBufferInfo.size  = dataSize;
    stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo stagingAllocInfo{};
    stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                             VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer      stagingBuffer;
    VmaAllocation stagingAlloc;
    VmaAllocationInfo stagingInfo;
    vmaCreateBuffer(mAllocator, &stagingBufferInfo, &stagingAllocInfo,
                    &stagingBuffer, &stagingAlloc, &stagingInfo);
    std::memcpy(stagingInfo.pMappedData, paletteColors.data(), dataSize);

    VkCommandBuffer commandBuffer = beginOneTimeCommandBuffer();

    transitionImageLayout(commandBuffer, tex.image,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);

    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent      = {paletteSize, 1, 1};
    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, tex.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    transitionImageLayout(commandBuffer, tex.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);

    endOneTimeCommandBuffer(commandBuffer);
    vmaDestroyBuffer(mAllocator, stagingBuffer, stagingAlloc);
}

void VulkanBackend::freePaletteTexture(DTexture paletteTexture)
{
    freeTexture(paletteTexture);
}

void VulkanBackend::linkIndirectTextures(DTexture indexTexture, DTexture paletteTexture)
{
    auto indexIt   = mTextures.find(indexTexture.id);
    auto paletteIt = mTextures.find(paletteTexture.id);
    if (indexIt == mTextures.end() || paletteIt == mTextures.end()) return;

    VulkanTexture& indexTex   = indexIt->second;
    VulkanTexture& paletteTex = paletteIt->second;

    // Free the old single-binding descriptor set on the index texture.
    if (indexTex.descriptorSet != VK_NULL_HANDLE)
        vkFreeDescriptorSets(mDevice, mDescriptorPool, 1, &indexTex.descriptorSet);

    // Create a 2-binding descriptor set using the indirect layout.
    indexTex.descriptorSet = allocateAndUpdateIndirectDescriptorSet(
        indexTex.imageView, indexTex.sampler,
        paletteTex.imageView, paletteTex.sampler);
}

VkDescriptorSet VulkanBackend::allocateAndUpdateIndirectDescriptorSet(
    VkImageView indexView, VkSampler indexSampler,
    VkImageView paletteView, VkSampler paletteSampler) const
{
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = mDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &mIndirectDescriptorSetLayout;

    VkDescriptorSet descriptorSet;
    if (vkAllocateDescriptorSets(mDevice, &allocInfo, &descriptorSet) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate indirect descriptor set");

    VkDescriptorImageInfo indexImageInfo{};
    indexImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    indexImageInfo.imageView   = indexView;
    indexImageInfo.sampler     = indexSampler;

    VkDescriptorImageInfo paletteImageInfo{};
    paletteImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    paletteImageInfo.imageView   = paletteView;
    paletteImageInfo.sampler     = paletteSampler;

    std::array<VkWriteDescriptorSet, 2> writes{};
    writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet          = descriptorSet;
    writes[0].dstBinding      = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].pImageInfo      = &indexImageInfo;

    writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet          = descriptorSet;
    writes[1].dstBinding      = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].pImageInfo      = &paletteImageInfo;

    vkUpdateDescriptorSets(mDevice, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    return descriptorSet;
}

// ---------------------------------------------------------------------------
// uploadMesh()
// ---------------------------------------------------------------------------

DMesh VulkanBackend::uploadMesh(const Mesh& mesh)
{
    const VkDeviceSize vertexSize = mesh.vertices.size() * sizeof(float);
    const VkDeviceSize indexSize  = mesh.indices.size()  * sizeof(unsigned int);

    auto uploadBuffer = [&](const void* data, VkDeviceSize size, VkBufferUsageFlags usage,
                             VkBuffer& outBuffer, VmaAllocation& outAlloc)
    {
        // Staging
        VkBufferCreateInfo stagingInfo{};
        stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        stagingInfo.size  = size;
        stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        VmaAllocationCreateInfo stagingAllocInfo{};
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                 VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VkBuffer stagingBuffer; VmaAllocation stagingAlloc; VmaAllocationInfo stagingMapped;
        vmaCreateBuffer(mAllocator, &stagingInfo, &stagingAllocInfo,
                        &stagingBuffer, &stagingAlloc, &stagingMapped);
        std::memcpy(stagingMapped.pMappedData, data, size);

        // Device-local
        VkBufferCreateInfo deviceInfo{};
        deviceInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        deviceInfo.size  = size;
        deviceInfo.usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VmaAllocationCreateInfo deviceAllocInfo{};
        deviceAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        vmaCreateBuffer(mAllocator, &deviceInfo, &deviceAllocInfo, &outBuffer, &outAlloc, nullptr);

        VkCommandBuffer commandBuffer = beginOneTimeCommandBuffer();
        VkBufferCopy copy{0, 0, size};
        vkCmdCopyBuffer(commandBuffer, stagingBuffer, outBuffer, 1, &copy);
        endOneTimeCommandBuffer(commandBuffer);

        vmaDestroyBuffer(mAllocator, stagingBuffer, stagingAlloc);
    };

    VkBuffer      vertexBuffer; VmaAllocation vertexAlloc;
    VkBuffer      indexBuffer;  VmaAllocation indexAlloc;
    uploadBuffer(mesh.vertices.data(), vertexSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertexBuffer, vertexAlloc);
    uploadBuffer(mesh.indices.data(),  indexSize,  VK_BUFFER_USAGE_INDEX_BUFFER_BIT,  indexBuffer,  indexAlloc);

    const std::size_t newId = mNextId++;
    mMeshes[newId] = VulkanMesh{vertexBuffer, vertexAlloc, indexBuffer, indexAlloc,
                                static_cast<uint32_t>(mesh.indices.size())};
    return DMesh{newId};
}

// ---------------------------------------------------------------------------
// freeMesh()
// ---------------------------------------------------------------------------

void VulkanBackend::freeMesh(DMesh dmesh)
{
    auto it = mMeshes.find(dmesh.id);
    if (it == mMeshes.end()) return;

    VulkanMesh& mesh = it->second;
    // Defer destruction into the slot of the most recently SUBMITTED frame.
    // That slot's fence covers the last GPU submission, so by the time beginFrame()
    // comes back to this slot it has waited for all in-flight work to complete.
    // Using mCurrentFrame directly is wrong when free is called from processInputs()
    // (before beginFrame), because mCurrentFrame already points to the upcoming slot
    // whose fence only covers work two frames back — not the just-submitted frame.
    const int lastSubmittedSlot = (mCurrentFrame - 1 + MAX_FRAMES_IN_FLIGHT) % MAX_FRAMES_IN_FLIGHT;
    FrameData& frame = mFrames[lastSubmittedSlot];
    frame.pendingBufferDeletions.push_back({mesh.vertexBuffer, mesh.vertexAlloc});
    frame.pendingBufferDeletions.push_back({mesh.indexBuffer,  mesh.indexAlloc});
    mMeshes.erase(it);
}

// ---------------------------------------------------------------------------
// createRenderTarget()
// ---------------------------------------------------------------------------

DRenderTarget VulkanBackend::createRenderTarget(glm::ivec2 size)
{
    const uint32_t width  = static_cast<uint32_t>(size.x);
    const uint32_t height = static_cast<uint32_t>(size.y);

    // Color image
    VkImageCreateInfo colorInfo{};
    colorInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    colorInfo.imageType     = VK_IMAGE_TYPE_2D;
    colorInfo.format        = VK_FORMAT_R8G8B8A8_UNORM;
    colorInfo.extent        = {width, height, 1};
    colorInfo.mipLevels     = 1;
    colorInfo.arrayLayers   = 1;
    colorInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    colorInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    colorInfo.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    colorInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo colorAllocInfo{};
    colorAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    VkImage colorImage; VmaAllocation colorAlloc;
    vmaCreateImage(mAllocator, &colorInfo, &colorAllocInfo, &colorImage, &colorAlloc, nullptr);

    // Transition color image to SHADER_READ_ONLY so first proxy descriptor write is valid
    VkCommandBuffer commandBuffer = beginOneTimeCommandBuffer();
    transitionImageLayout(commandBuffer, colorImage,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, VK_ACCESS_SHADER_READ_BIT);
    endOneTimeCommandBuffer(commandBuffer);

    VkImageViewCreateInfo colorViewInfo{};
    colorViewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    colorViewInfo.image                           = colorImage;
    colorViewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    colorViewInfo.format                          = VK_FORMAT_R8G8B8A8_UNORM;
    colorViewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    colorViewInfo.subresourceRange.levelCount     = 1;
    colorViewInfo.subresourceRange.baseArrayLayer = 0;
    colorViewInfo.subresourceRange.layerCount     = 1;

    VkImageView colorView;
    vkCreateImageView(mDevice, &colorViewInfo, nullptr, &colorView);

    // Depth image
    VkImageCreateInfo depthInfo{};
    depthInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depthInfo.imageType     = VK_IMAGE_TYPE_2D;
    depthInfo.format        = mDepthFormat;
    depthInfo.extent        = {width, height, 1};
    depthInfo.mipLevels     = 1;
    depthInfo.arrayLayers   = 1;
    depthInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    depthInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    depthInfo.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depthInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo depthAllocInfo{};
    depthAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    VkImage depthImage; VmaAllocation depthAlloc;
    vmaCreateImage(mAllocator, &depthInfo, &depthAllocInfo, &depthImage, &depthAlloc, nullptr);

    const bool hasStencil = (mDepthFormat == VK_FORMAT_D24_UNORM_S8_UINT);
    VkImageViewCreateInfo depthViewInfo{};
    depthViewInfo.sType                       = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depthViewInfo.image                       = depthImage;
    depthViewInfo.viewType                    = VK_IMAGE_VIEW_TYPE_2D;
    depthViewInfo.format                      = mDepthFormat;
    depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT |
                                                (hasStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u);
    depthViewInfo.subresourceRange.levelCount = 1;
    depthViewInfo.subresourceRange.layerCount = 1;

    VkImageView depthView;
    vkCreateImageView(mDevice, &depthViewInfo, nullptr, &depthView);

    // Framebuffer
    std::array<VkImageView, 2> attachments = {colorView, depthView};
    VkFramebufferCreateInfo fbInfo{};
    fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass      = mRttRenderPass;
    fbInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    fbInfo.pAttachments    = attachments.data();
    fbInfo.width           = width;
    fbInfo.height          = height;
    fbInfo.layers          = 1;

    VkFramebuffer framebuffer;
    vkCreateFramebuffer(mDevice, &fbInfo, nullptr, &framebuffer);

    const std::size_t newId = mNextId++;
    mRenderTargets[newId] = VulkanRenderTarget{
        colorImage, colorAlloc, colorView,
        depthImage, depthAlloc, depthView,
        framebuffer, size, 0
    };
    return DRenderTarget{newId, size};
}

// ---------------------------------------------------------------------------
// getRenderTargetTexture()
// ---------------------------------------------------------------------------

DTexture VulkanBackend::getRenderTargetTexture(DRenderTarget renderTarget)
{
    VulkanRenderTarget& rt = mRenderTargets.at(renderTarget.id);

    VkSampler       sampler       = createSampler(TextureSampleMode::Nearest, TextureSampleMode::Nearest);
    VkDescriptorSet descriptorSet = allocateAndUpdateDescriptorSet(rt.colorView, sampler);

    const std::size_t newId = mNextId++;
    // isProxy = true: image/allocation owned by the render target, not freed on freeTexture
    mTextures[newId] = VulkanTexture{rt.colorImage, VK_NULL_HANDLE, rt.colorView, sampler, descriptorSet, true};
    rt.proxyTextureId = newId;

    return DTexture{newId};
}

// ---------------------------------------------------------------------------
// freeRenderTarget()
// ---------------------------------------------------------------------------

void VulkanBackend::freeRenderTarget(DRenderTarget renderTarget, DTexture proxyTexture)
{
    auto rtIt = mRenderTargets.find(renderTarget.id);
    if (rtIt == mRenderTargets.end()) return;

    // Collect proxy texture fields — image is owned by the RT, not the texture entry
    VkDescriptorSet proxyDescriptorSet = VK_NULL_HANDLE;
    VkSampler       proxySampler       = VK_NULL_HANDLE;
    auto texIt = mTextures.find(proxyTexture.id);
    if (texIt != mTextures.end())
    {
        proxyDescriptorSet = texIt->second.descriptorSet;
        proxySampler       = texIt->second.sampler;
        mTextures.erase(texIt);
    }

    VulkanRenderTarget& rt = rtIt->second;
    const int lastSubmittedSlot = (mCurrentFrame - 1 + MAX_FRAMES_IN_FLIGHT) % MAX_FRAMES_IN_FLIGHT;
    mFrames[lastSubmittedSlot].pendingRenderTargetDeletions.push_back({
        proxyDescriptorSet, proxySampler,
        rt.framebuffer,
        rt.colorView, rt.depthView,
        rt.colorImage, rt.colorAlloc,
        rt.depthImage, rt.depthAlloc
    });
    mRenderTargets.erase(rtIt);
}

// ---------------------------------------------------------------------------
// beginFrame()
// ---------------------------------------------------------------------------

void VulkanBackend::beginFrame(
    glm::vec3 clearColor, ViewportRect gameViewport, int framebufferWidth, int framebufferHeight)
{
    mClearColor               = clearColor;
    mCurrentGameViewport      = gameViewport;
    mCurrentFramebufferWidth  = framebufferWidth;
    mCurrentFramebufferHeight = framebufferHeight;

    FrameData& frame = mFrames[mCurrentFrame];

    vkWaitForFences(mDevice, 1, &frame.inFlight, VK_TRUE, UINT64_MAX);

    // Safe to destroy resources queued during the previous use of this frame slot —
    // the fence above guarantees the GPU has finished executing that submission.
    flushPendingDeletions(frame);

    AcquireResult acquireResult = mPresentation.acquireImage(mDevice);
    if (acquireResult == AcquireResult::Recreated)
    {
        mActiveCommandBuffer = VK_NULL_HANDLE;
        return;
    }

    vkResetFences(mDevice, 1, &frame.inFlight);

    VkCommandBuffer commandBuffer = frame.commandBuffer;
    vkResetCommandBuffer(commandBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    mActiveCommandBuffer = commandBuffer;
}

// ---------------------------------------------------------------------------
// imguiNewFrame()
// ---------------------------------------------------------------------------

void VulkanBackend::imguiNewFrame()
{
    ImGui_ImplVulkan_NewFrame();
}

// ---------------------------------------------------------------------------
// beginRttPass()
// ---------------------------------------------------------------------------

void VulkanBackend::beginRttPass(DRenderTarget renderTarget, glm::vec4 clearColor)
{
    if (mActiveCommandBuffer == VK_NULL_HANDLE) return;

    VulkanRenderTarget& rt = mRenderTargets.at(renderTarget.id);
    mActiveRttRenderTargetId = renderTarget.id;

    // Barrier: SHADER_READ_ONLY → COLOR_ATTACHMENT
    transitionImageLayout(mActiveCommandBuffer, rt.colorImage,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color        = {{clearColor.r, clearColor.g, clearColor.b, clearColor.a}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rpBegin{};
    rpBegin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.renderPass        = mRttRenderPass;
    rpBegin.framebuffer       = rt.framebuffer;
    rpBegin.renderArea.extent = {static_cast<uint32_t>(rt.size.x), static_cast<uint32_t>(rt.size.y)};
    rpBegin.clearValueCount   = static_cast<uint32_t>(clearValues.size());
    rpBegin.pClearValues      = clearValues.data();
    vkCmdBeginRenderPass(mActiveCommandBuffer, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

    // Negative-height viewport for Y-up convention, same as beginMainPass.
    VkViewport vp{};
    vp.x        = 0.0f;
    vp.y        = static_cast<float>(rt.size.y);
    vp.width    = static_cast<float>(rt.size.x);
    vp.height   = -static_cast<float>(rt.size.y);
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(mActiveCommandBuffer, 0, 1, &vp);

    VkRect2D scissor{};
    scissor.extent = {static_cast<uint32_t>(rt.size.x), static_cast<uint32_t>(rt.size.y)};
    vkCmdSetScissor(mActiveCommandBuffer, 0, 1, &scissor);

    vkCmdBindPipeline(mActiveCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mSpritePipeline);
}

// ---------------------------------------------------------------------------
// endRttPass()
// ---------------------------------------------------------------------------

void VulkanBackend::endRttPass()
{
    if (mActiveCommandBuffer == VK_NULL_HANDLE) return;

    vkCmdEndRenderPass(mActiveCommandBuffer);

    VulkanRenderTarget& rt = mRenderTargets.at(mActiveRttRenderTargetId);

    // Barrier: COLOR_ATTACHMENT → SHADER_READ_ONLY
    transitionImageLayout(mActiveCommandBuffer, rt.colorImage,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);

    mActiveRttRenderTargetId = 0;
}

// ---------------------------------------------------------------------------
// beginMainPass()
// ---------------------------------------------------------------------------

void VulkanBackend::beginMainPass(ViewportRect gameViewport)
{
    if (mActiveCommandBuffer == VK_NULL_HANDLE) return;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color        = {{0.0f, 0.0f, 0.0f, 1.0f}};  // letterbox black via LOAD_OP_CLEAR
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rpBegin{};
    rpBegin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.renderPass        = mMainRenderPass;
    rpBegin.framebuffer       = mPresentation.mainFramebuffer();
    rpBegin.renderArea.extent = mPresentation.extent();
    rpBegin.clearValueCount   = static_cast<uint32_t>(clearValues.size());
    rpBegin.pClearValues      = clearValues.data();
    vkCmdBeginRenderPass(mActiveCommandBuffer, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

    // Clear game area to canvas clear color (letterbox bands remain black from LOAD_OP_CLEAR)
    if (mClearColor != glm::vec3{0.0f, 0.0f, 0.0f})
    {
        VkClearAttachment clearAttachment{};
        clearAttachment.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT;
        clearAttachment.colorAttachment = 0;
        clearAttachment.clearValue.color = {{mClearColor.r, mClearColor.g, mClearColor.b, 1.0f}};

        VkClearRect clearRect{};
        // y from bottom (OpenGL) → y from top (Vulkan)
        int32_t clearX = gameViewport.x;
        int32_t clearY = mCurrentFramebufferHeight - gameViewport.y - gameViewport.height;
        int32_t clearW = gameViewport.width;
        int32_t clearH = gameViewport.height;

        // Clamp to render area to avoid validation errors during resize.
        const VkExtent2D presentExtent = mPresentation.extent();
        const int32_t renderW = static_cast<int32_t>(presentExtent.width);
        const int32_t renderH = static_cast<int32_t>(presentExtent.height);
        if (clearX < 0) { clearW += clearX; clearX = 0; }
        if (clearY < 0) { clearH += clearY; clearY = 0; }
        if (clearX + clearW > renderW) clearW = renderW - clearX;
        if (clearY + clearH > renderH) clearH = renderH - clearY;

        if (clearW > 0 && clearH > 0)
        {
            clearRect.rect.offset    = {clearX, clearY};
            clearRect.rect.extent    = {static_cast<uint32_t>(clearW), static_cast<uint32_t>(clearH)};
            clearRect.baseArrayLayer = 0;
            clearRect.layerCount     = 1;
            vkCmdClearAttachments(mActiveCommandBuffer, 1, &clearAttachment, 1, &clearRect);
        }
    }

    // Negative-height viewport: maps NDC y=-1 → bottom, y=+1 → top, matching
    // OpenGL convention. ViewportRect.y is the bottom edge in framebuffer pixels
    // (OpenGL convention); converting to Vulkan top-of-viewport = framebufferHeight - y.
    VkViewport vp{};
    vp.x        = static_cast<float>(gameViewport.x);
    vp.y        = static_cast<float>(mCurrentFramebufferHeight - gameViewport.y);
    vp.width    = static_cast<float>(gameViewport.width);
    vp.height   = -static_cast<float>(gameViewport.height);
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(mActiveCommandBuffer, 0, 1, &vp);

    VkRect2D scissor{};
    scissor.offset = {gameViewport.x, static_cast<int32_t>(mCurrentFramebufferHeight - gameViewport.y - gameViewport.height)};
    scissor.extent = {static_cast<uint32_t>(gameViewport.width), static_cast<uint32_t>(gameViewport.height)};
    vkCmdSetScissor(mActiveCommandBuffer, 0, 1, &scissor);

    vkCmdBindPipeline(mActiveCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mSpritePipeline);
}

// ---------------------------------------------------------------------------
// drawSprite()
// ---------------------------------------------------------------------------

void VulkanBackend::drawSprite(DMesh dmesh, DTexture dtexture, const SpriteDrawParams& params)
{
    if (mActiveCommandBuffer == VK_NULL_HANDLE) return;

    const VulkanMesh&    mesh = mMeshes.at(dmesh.id);
    const VulkanTexture& tex  = mTextures.at(dtexture.id);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(mActiveCommandBuffer, 0, 1, &mesh.vertexBuffer, &offset);
    vkCmdBindIndexBuffer(mActiveCommandBuffer, mesh.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    if (params.isIndirect)
    {
        vkCmdBindPipeline(mActiveCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mIndirectSpritePipeline);
        vkCmdBindDescriptorSets(mActiveCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                mIndirectPipelineLayout, 0, 1, &tex.descriptorSet, 0, nullptr);

        const SpritePushConstants pushConstants = packPushConstants(params);
        vkCmdPushConstants(mActiveCommandBuffer, mIndirectPipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(SpritePushConstants), &pushConstants);
    }
    else
    {
        vkCmdBindPipeline(mActiveCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mSpritePipeline);
        vkCmdBindDescriptorSets(mActiveCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                mPipelineLayout, 0, 1, &tex.descriptorSet, 0, nullptr);

        const SpritePushConstants pushConstants = packPushConstants(params);
        vkCmdPushConstants(mActiveCommandBuffer, mPipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(SpritePushConstants), &pushConstants);
    }

    vkCmdDrawIndexed(mActiveCommandBuffer, mesh.indexCount, 1, 0, 0, 0);
}

// ---------------------------------------------------------------------------
// endFrame()
// ---------------------------------------------------------------------------

void VulkanBackend::endFrame(
    ImDrawData* imguiData, int framebufferWidth, int framebufferHeight)
{
    if (mActiveCommandBuffer == VK_NULL_HANDLE) return;

    // Restore full-framebuffer viewport and scissor for ImGui
    VkViewport fullVp{0, 0, static_cast<float>(framebufferWidth), static_cast<float>(framebufferHeight), 0.0f, 1.0f};
    VkRect2D   fullScissor{{0, 0}, {static_cast<uint32_t>(framebufferWidth), static_cast<uint32_t>(framebufferHeight)}};
    vkCmdSetViewport(mActiveCommandBuffer, 0, 1, &fullVp);
    vkCmdSetScissor(mActiveCommandBuffer, 0, 1, &fullScissor);

    ImGui_ImplVulkan_RenderDrawData(imguiData, mActiveCommandBuffer);

    vkCmdEndRenderPass(mActiveCommandBuffer);
    vkEndCommandBuffer(mActiveCommandBuffer);

    FrameData& frame = mFrames[mCurrentFrame];
    mPresentation.submitAndPresent(mDevice, mGraphicsQueue, frame.commandBuffer, frame.inFlight);

    mCurrentFrame = (mCurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    mActiveCommandBuffer = VK_NULL_HANDLE;
}

// ---------------------------------------------------------------------------
// takeScreenshot()
// ---------------------------------------------------------------------------

ScreenshotPixels VulkanBackend::takeScreenshot(ViewportRect gameViewport, glm::ivec2 gameSize) const
{
    return mPresentation.takeScreenshot(mDevice, mAllocator, mCommandPool, mGraphicsQueue,
                                        gameViewport, gameSize);
}

// ---------------------------------------------------------------------------
// setTextureMinFilter() / setTextureMagFilter()
// ---------------------------------------------------------------------------

void VulkanBackend::rebuildSampler(
    VulkanTexture& tex, TextureSampleMode minFilter, TextureSampleMode magFilter)
{
    if (tex.isProxy) return;  // proxy sampler is not user-configurable

    vkFreeDescriptorSets(mDevice, mDescriptorPool, 1, &tex.descriptorSet);
    vkDestroySampler(mDevice, tex.sampler, nullptr);

    tex.sampler       = createSampler(minFilter, magFilter);
    tex.descriptorSet = allocateAndUpdateDescriptorSet(tex.imageView, tex.sampler);
}

void VulkanBackend::setTextureMinFilter(DTexture dtexture, TextureSampleMode mode)
{
    auto it = mTextures.find(dtexture.id);
    if (it == mTextures.end()) return;
    // Reconstruct sampler: we don't cache the current mag filter, so use Nearest as default
    // and rely on setTextureMagFilter being called separately if needed.
    rebuildSampler(it->second, mode, TextureSampleMode::Nearest);
}

void VulkanBackend::setTextureMagFilter(DTexture dtexture, TextureSampleMode mode)
{
    auto it = mTextures.find(dtexture.id);
    if (it == mTextures.end()) return;
    rebuildSampler(it->second, TextureSampleMode::Nearest, mode);
}

} // namespace Nothofagus
