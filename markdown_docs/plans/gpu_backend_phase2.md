# GPU Backend Abstraction — Phase 2: Vulkan Backend

## Overview

Implement `VulkanBackend` as a drop-in replacement for `OpenGLBackend`, satisfying the `RenderBackend<T>` concept. The compile-time dispatch in `canvas_impl.h` is already wired — `#if defined(NOTHOFAGUS_BACKEND_VULKAN)` already selects `VulkanBackend` — so all work is purely additive.

---

## 1. New Submodules / Dependencies

### Libraries to add as git submodules

**vk-bootstrap** — `https://github.com/charles-lunarg/vk-bootstrap`
- Handles `VkInstance`, `VkPhysicalDevice`, `VkDevice`, and `VkSwapchainKHR` creation with sane defaults. Eliminates ~600 lines of boilerplate from `initialize()`.

**VulkanMemoryAllocator (VMA)** — `https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator`
- Manages `VkBuffer` and `VkImage` memory via a `VmaAllocator`. Required for vertex/index buffers, texture images, depth images, and screenshot staging buffers.

```bash
git submodule add https://github.com/charles-lunarg/vk-bootstrap                       third_party/vk-bootstrap
git submodule add https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator    third_party/VulkanMemoryAllocator
```

### Raw Vulkan C API (not Vulkan-Hpp)

Use the **raw Vulkan C API**. Justification:
- `imgui_impl_vulkan.h` (already in-tree) uses the C API — mixing C and C++ wrapper types causes friction.
- VMA and vk-bootstrap both expose C-API handles natively.
- The C API is more explicit about ownership, which matters for the proxy-texture pattern.
- `UniqueHandle` RAII from Vulkan-Hpp conflicts with the existing `IndexedContainer` lifetime model.

### Vulkan headers / loader

Use `find_package(Vulkan REQUIRED)` (system Vulkan SDK). `Vulkan::Vulkan` provides headers + the loader import lib. No SDK bundled in-tree.

---

## 2. Shader Files and SPIR-V Compilation

### File locations

```
source/shaders/
    sprite.vert
    sprite.frag
    sprite.vert.spv   (generated at build time — gitignored)
    sprite.frag.spv   (generated at build time — gitignored)
```

### `sprite.vert`

```glsl
#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec2 outTexCoord;

layout(push_constant) uniform PushConstants {
    mat3  transform;      // offset 0,  size 48 (each column padded to vec4)
    int   layerIndex;     // offset 48, size 4
    vec3  tintColor;      // offset 52, size 12
    float tintIntensity;  // offset 64, size 4
    float opacity;        // offset 68, size 4
    // Total: 72 bytes
} pc;

void main()
{
    outTexCoord = inTexCoord;
    vec3 worldPos = pc.transform * vec3(inPosition, 1.0);
    gl_Position = vec4(worldPos.xy, 0.0, 1.0);
}
```

### `sprite.frag`

```glsl
#version 450

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2DArray textureSampler;

layout(push_constant) uniform PushConstants {
    mat3  transform;
    int   layerIndex;
    vec3  tintColor;
    float tintIntensity;
    float opacity;
} pc;

void main()
{
    vec4  s          = texture(textureSampler, vec3(inTexCoord, float(pc.layerIndex)));
    vec3  blendColor = pc.tintColor * pc.tintIntensity + s.rgb * (1.0 - pc.tintIntensity);
    outColor = vec4(blendColor, s.a * pc.opacity);
}
```

### Push constant alignment — the mat3 problem

In Vulkan GLSL, `mat3` occupies **48 bytes** because each column is stored as `vec4` (16 bytes). The CPU-side `glm::mat3` is 36 bytes. **Never memcpy a `glm::mat3` directly into push constants.** Use an explicit struct with padding:

```cpp
struct SpritePushConstants {
    float col0[3]; float _pad0;  // mat3 column 0
    float col1[3]; float _pad1;  // mat3 column 1
    float col2[3]; float _pad2;  // mat3 column 2
    int   layerIndex;
    float tintColor[3];
    float tintIntensity;
    float opacity;
};
static_assert(sizeof(SpritePushConstants) == 72);

static SpritePushConstants packPushConstants(const SpriteDrawParams& p)
{
    SpritePushConstants pc{};
    pc.col0[0] = p.transform[0][0]; pc.col0[1] = p.transform[0][1]; pc.col0[2] = p.transform[0][2];
    pc.col1[0] = p.transform[1][0]; pc.col1[1] = p.transform[1][1]; pc.col1[2] = p.transform[1][2];
    pc.col2[0] = p.transform[2][0]; pc.col2[1] = p.transform[2][1]; pc.col2[2] = p.transform[2][2];
    pc.layerIndex    = p.layerIndex;
    pc.tintColor[0]  = p.tintColor.r;
    pc.tintColor[1]  = p.tintColor.g;
    pc.tintColor[2]  = p.tintColor.b;
    pc.tintIntensity = p.tintIntensity;
    pc.opacity       = p.opacity;
    return pc;
}
```

### CMake SPIR-V commands

```cmake
find_program(GLSLC glslc REQUIRED)
set(SHADER_VERT_SRC "${CMAKE_CURRENT_SOURCE_DIR}/source/shaders/sprite.vert")
set(SHADER_FRAG_SRC "${CMAKE_CURRENT_SOURCE_DIR}/source/shaders/sprite.frag")
set(SHADER_VERT_SPV "${CMAKE_CURRENT_BINARY_DIR}/sprite.vert.spv")
set(SHADER_FRAG_SPV "${CMAKE_CURRENT_BINARY_DIR}/sprite.frag.spv")

add_custom_command(OUTPUT "${SHADER_VERT_SPV}"
    COMMAND "${GLSLC}" "${SHADER_VERT_SRC}" -o "${SHADER_VERT_SPV}"
    DEPENDS "${SHADER_VERT_SRC}" VERBATIM)
add_custom_command(OUTPUT "${SHADER_FRAG_SPV}"
    COMMAND "${GLSLC}" "${SHADER_FRAG_SRC}" -o "${SHADER_FRAG_SPV}"
    DEPENDS "${SHADER_FRAG_SRC}" VERBATIM)
add_custom_target(spirv_shaders DEPENDS "${SHADER_VERT_SPV}" "${SHADER_FRAG_SPV}")
add_dependencies(nothofagus spirv_shaders)

target_compile_definitions(nothofagus PRIVATE
    NOTHOFAGUS_SPRITE_VERT_SPV="${SHADER_VERT_SPV}"
    NOTHOFAGUS_SPRITE_FRAG_SPV="${SHADER_FRAG_SPV}")
```

The backend reads the SPIR-V paths at runtime via `std::ifstream`. The `cmake_current_binary_dir` paths are baked in as preprocessor string macros.

---

## 3. Internal Data Structures

### `VulkanMesh`

```cpp
struct VulkanMesh {
    VkBuffer      vertexBuffer = VK_NULL_HANDLE;
    VmaAllocation vertexAlloc  = VK_NULL_HANDLE;
    VkBuffer      indexBuffer  = VK_NULL_HANDLE;
    VmaAllocation indexAlloc   = VK_NULL_HANDLE;
    uint32_t      indexCount   = 0;
};
```

### `VulkanTexture`

```cpp
struct VulkanTexture {
    VkImage         image        = VK_NULL_HANDLE;
    VmaAllocation   allocation   = VK_NULL_HANDLE;  // null for proxy textures
    VkImageView     imageView    = VK_NULL_HANDLE;
    VkSampler       sampler      = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    bool            isProxy      = false;  // owned by VulkanRenderTarget — don't free image/allocation
};
```

### `VulkanRenderTarget`

```cpp
struct VulkanRenderTarget {
    VkImage       colorImage  = VK_NULL_HANDLE;
    VmaAllocation colorAlloc  = VK_NULL_HANDLE;
    VkImageView   colorView   = VK_NULL_HANDLE;

    VkImage       depthImage  = VK_NULL_HANDLE;
    VmaAllocation depthAlloc  = VK_NULL_HANDLE;
    VkImageView   depthView   = VK_NULL_HANDLE;

    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    glm::ivec2    size        = {0, 0};
    std::size_t   proxyTextureId = 0;  // set by getRenderTargetTexture
};
```

Color image: `VK_FORMAT_R8G8B8A8_UNORM`, usage `COLOR_ATTACHMENT | SAMPLED`.
Depth image: `VK_FORMAT_D24_UNORM_S8_UINT` (fallback `D32_SFLOAT`), usage `DEPTH_STENCIL_ATTACHMENT`.

### `FrameData` (per-frame-in-flight)

```cpp
static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

struct FrameData {
    VkCommandBuffer commandBuffer  = VK_NULL_HANDLE;
    VkSemaphore     imageAvailable = VK_NULL_HANDLE;
    VkSemaphore     renderFinished = VK_NULL_HANDLE;
    VkFence         inFlight       = VK_NULL_HANDLE;
};
```

---

## 4. `VulkanBackend` Class Members

```cpp
class VulkanBackend {
    // vk-bootstrap
    VkInstance               mInstance        = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT mDebugMessenger  = VK_NULL_HANDLE;
    VkSurfaceKHR             mSurface         = VK_NULL_HANDLE;
    VkPhysicalDevice         mPhysicalDevice  = VK_NULL_HANDLE;
    VkDevice                 mDevice          = VK_NULL_HANDLE;
    VkQueue                  mGraphicsQueue   = VK_NULL_HANDLE;
    VkQueue                  mPresentQueue    = VK_NULL_HANDLE;
    uint32_t                 mGraphicsQueueFamily = 0;

    // Swapchain
    VkSwapchainKHR             mSwapchain     = VK_NULL_HANDLE;
    VkFormat                   mSwapchainFormat;
    VkExtent2D                 mSwapchainExtent;
    std::vector<VkImage>       mSwapchainImages;
    std::vector<VkImageView>   mSwapchainImageViews;
    std::vector<VkFramebuffer> mSwapchainFramebuffers;

    // Render passes
    VkRenderPass  mMainRenderPass = VK_NULL_HANDLE;  // swapchain target
    VkRenderPass  mRttRenderPass  = VK_NULL_HANDLE;  // off-screen target

    // Pipeline
    VkDescriptorSetLayout mDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout      mPipelineLayout      = VK_NULL_HANDLE;
    VkPipeline            mSpritePipeline      = VK_NULL_HANDLE;

    // Descriptor pools (separate: sprites vs ImGui)
    VkDescriptorPool mDescriptorPool      = VK_NULL_HANDLE;
    VkDescriptorPool mImguiDescriptorPool = VK_NULL_HANDLE;

    // Commands + per-frame sync
    VkCommandPool mCommandPool = VK_NULL_HANDLE;
    std::array<FrameData, MAX_FRAMES_IN_FLIGHT> mFrames;
    int mCurrentFrame = 0;

    // VMA
    VmaAllocator mAllocator = VK_NULL_HANDLE;

    // Resource maps
    std::unordered_map<std::size_t, VulkanMesh>         mMeshes;
    std::unordered_map<std::size_t, VulkanTexture>      mTextures;
    std::unordered_map<std::size_t, VulkanRenderTarget> mRenderTargets;
    std::size_t mNextId = 0;

    // Per-frame state (set in beginFrame, consumed in drawSprite/endFrame)
    uint32_t        mCurrentSwapchainImageIndex = 0;
    VkCommandBuffer mActiveCommandBuffer        = VK_NULL_HANDLE;
    ViewportRect    mCurrentGameViewport        = {};
    int             mFramebufferWidth           = 0;
    int             mFramebufferHeight          = 0;
};
```

---

## 5. Method-by-Method Implementation Notes

### `initialize(GLFWwindow* window, glm::ivec2 canvasSize)`

1. `vkb::InstanceBuilder` — enable validation layers + `VK_EXT_debug_utils` in debug builds.
2. `glfwCreateWindowSurface(mInstance, window, nullptr, &mSurface)`.
3. `vkb::PhysicalDeviceSelector` — require `samplerAnisotropy`, `VK_KHR_swapchain`, `VK_FORMAT_R8G8B8A8_UNORM` color attachment support.
4. `vkb::DeviceBuilder` → `mDevice`, `mGraphicsQueue`, `mPresentQueue`, `mGraphicsQueueFamily`.
5. `vmaCreateAllocator` referencing `mInstance`, `mPhysicalDevice`, `mDevice`.
6. `vkb::SwapchainBuilder` — prefer `VK_FORMAT_R8G8B8A8_UNORM`, `FIFO` present mode.
7. Create `VkImageView`s for each swapchain image.
8. Create `VkCommandPool` with `VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT`.
9. Allocate `MAX_FRAMES_IN_FLIGHT` command buffers.
10. Create per-frame semaphores and fences (fences created with `VK_FENCE_CREATE_SIGNALED_BIT`).
11. Create `mMainRenderPass` and `mRttRenderPass` (see Render Pass Design).
12. Create swapchain `VkFramebuffer`s using `mMainRenderPass`.
13. Create descriptor set layout: binding 0 = `COMBINED_IMAGE_SAMPLER`, `FRAGMENT_SHADER`.
14. Create `mDescriptorPool` with `VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT`, 1024 sets.
15. Create `mImguiDescriptorPool` (sized per ImGui requirements).
16. Load SPIR-V from `NOTHOFAGUS_SPRITE_VERT_SPV` / `NOTHOFAGUS_SPRITE_FRAG_SPV`, create shader modules.
17. Create `VkPipelineLayout` — push constant range: `VERTEX | FRAGMENT`, offset 0, `sizeof(SpritePushConstants)`.
18. Create `mSpritePipeline`:
    - Vertex input: binding 0, stride 16, two attributes — location 0 `R32G32_SFLOAT` offset 0, location 1 `R32G32_SFLOAT` offset 8.
    - Input assembly: `TRIANGLE_LIST`.
    - Viewport/scissor: dynamic state.
    - Rasterization: `FILL`, cull `NONE`.
    - Depth/stencil: disabled (depth-sorting is CPU-side).
    - Blend: `SRC_ALPHA / ONE_MINUS_SRC_ALPHA` — matches OpenGL `glBlendFunc`.
    - Render pass: `mMainRenderPass`, subpass 0.
19. Destroy shader modules.
20. `ImGui_ImplGlfw_InitForVulkan(window, true)`.
21. `ImGui_ImplVulkan_Init(...)` with `mMainRenderPass`, `mImguiDescriptorPool`, image count.
22. Upload ImGui fonts: one-shot command buffer → `ImGui_ImplVulkan_CreateFontsTexture()` → submit → `vkQueueWaitIdle`.

---

### `shutdown()`

1. `vkDeviceWaitIdle(mDevice)`.
2. `ImGui_ImplVulkan_Shutdown()`, `ImGui_ImplGlfw_Shutdown()`.
3. Destroy all resource map entries (meshes, textures, render targets).
4. Destroy pipeline, pipeline layout, descriptor set layout, descriptor pools.
5. Destroy `mRttRenderPass`, `mMainRenderPass`.
6. Destroy swapchain framebuffers and image views.
7. `vkDestroySwapchainKHR`.
8. Destroy per-frame fences and semaphores.
9. `vkDestroyCommandPool`.
10. `vmaDestroyAllocator`.
11. `vkDestroyDevice` → `vkDestroySurfaceKHR` → debug messenger → `vkDestroyInstance`.

---

### `uploadTexture(const Texture&, TextureSampleMode minFilter, TextureSampleMode magFilter)` → `DTexture`

1. `std::visit(GenerateTextureDataVisitor(), texture)` → `TextureData` (RGBA8, width × height × layers).
2. Create host-visible staging buffer (`TRANSFER_SRC`, `VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT`). Memcpy pixel data.
3. Create device-local `VkImage` — `2D`, `VK_FORMAT_R8G8B8A8_UNORM`, `arrayLayers = layers`, `TRANSFER_DST | SAMPLED`, `VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE`.
4. Single-use command buffer:
   - Barrier: `UNDEFINED → TRANSFER_DST_OPTIMAL`.
   - `vkCmdCopyBufferToImage` — `layerCount = layers`.
   - Barrier: `TRANSFER_DST_OPTIMAL → SHADER_READ_ONLY_OPTIMAL`.
5. Destroy staging buffer.
6. Create `VkImageView` — `2D_ARRAY`, `layerCount = layers`.
7. Create `VkSampler` — `minFilter` / `magFilter` mapped from `TextureSampleMode`, `ADDRESS_MODE_REPEAT`.
8. Allocate `VkDescriptorSet` from `mDescriptorPool`, update with `COMBINED_IMAGE_SAMPLER`.
9. Store `VulkanTexture{image, allocation, imageView, sampler, descriptorSet, false}` in `mTextures[newId]`.

---

### `freeTexture(DTexture dtexture)`

1. `vkFreeDescriptorSets`, `vkDestroySampler`, `vkDestroyImageView`.
2. If `!tex.isProxy`: `vmaDestroyImage(mAllocator, tex.image, tex.allocation)`.
3. Erase from `mTextures`.

---

### `uploadMesh(const Mesh& mesh)` → `DMesh`

1. Create staging buffers for vertices and indices (`TRANSFER_SRC`, host-visible). Memcpy data.
2. Create device-local vertex buffer (`VERTEX_BUFFER | TRANSFER_DST`) and index buffer (`INDEX_BUFFER | TRANSFER_DST`).
3. Single-use command buffer: two `vkCmdCopyBuffer` calls.
4. Destroy staging buffers.
5. Store `VulkanMesh{vBuf, vAlloc, iBuf, iAlloc, indexCount}` in `mMeshes[newId]`.

---

### `freeMesh(DMesh dmesh)`

`vmaDestroyBuffer` for vertex and index buffers. Erase from `mMeshes`.

---

### `createRenderTarget(glm::ivec2 targetSize)` → `DRenderTarget`

1. Create color image: `R8G8B8A8_UNORM`, `COLOR_ATTACHMENT | SAMPLED`, `arrayLayers = 1`.
2. Transition to `SHADER_READ_ONLY_OPTIMAL` via single-use command buffer.
3. Create color `VkImageView`: `2D_ARRAY`, `layerCount = 1`.
4. Create depth image: query `D24_UNORM_S8_UINT` support (fallback `D32_SFLOAT`), `DEPTH_STENCIL_ATTACHMENT`.
5. Create depth view.
6. Create `VkFramebuffer` using `mRttRenderPass`, attaching color and depth views.
7. Store `VulkanRenderTarget` in `mRenderTargets[newId]`. Return `DRenderTarget{newId, targetSize}`.

---

### `getRenderTargetTexture(DRenderTarget renderTarget)` → `DTexture`

1. Lookup `VulkanRenderTarget& rt`.
2. Create `VkSampler` with `NEAREST` filter (matching OpenGL RTT default).
3. Allocate `VkDescriptorSet` from `mDescriptorPool`. Update with `rt.colorView`, layout = `SHADER_READ_ONLY_OPTIMAL`.
4. Create proxy `VulkanTexture{rt.colorImage, VK_NULL_HANDLE, rt.colorView, sampler, descriptorSet, isProxy=true}`.
5. Insert into `mTextures[newId]`. Set `rt.proxyTextureId = newId`.
6. Return `DTexture{newId}`.

---

### `freeRenderTarget(DRenderTarget renderTarget, DTexture proxyTexture)`

1. From `mTextures[proxyTexture.id]`: free descriptor set, sampler, imageView. **Do NOT call `vmaDestroyImage`** — the image is owned by the render target. Erase from `mTextures`.
2. Lookup `VulkanRenderTarget& rt`.
3. `vkDestroyFramebuffer`, destroy color/depth image views, `vmaDestroyImage` for both color and depth.
4. Erase from `mRenderTargets`.

---

### Frame loop — revised design

`canvas_impl.cpp` calls in this order:
```
beginFrame → imguiNewFrame → [beginRttPass / drawSprite / endRttPass]* → beginMainPass → drawSprite* → endFrame
```

This maps cleanly to:
- **`beginFrame`**: acquire swapchain image, wait/reset fence, reset + begin command buffer. No render pass opened yet.
- **`imguiNewFrame`**: `ImGui_ImplVulkan_NewFrame()` (CPU only).
- **`beginRttPass`**: barrier `SHADER_READ_ONLY → COLOR_ATTACHMENT`, open `mRttRenderPass`, set full-RTT viewport/scissor, bind pipeline.
- **`endRttPass`**: close `mRttRenderPass`, barrier `COLOR_ATTACHMENT → SHADER_READ_ONLY`.
- **`beginMainPass`**: open `mMainRenderPass` (clear entire framebuffer to black via `LOAD_OP_CLEAR`, then `vkCmdClearAttachments` for game area color), set game viewport/scissor, bind pipeline.
- **`drawSprite`**: bind vertex/index buffers, bind descriptor set, push constants, `vkCmdDrawIndexed`.
- **`endFrame`**: restore full viewport/scissor, `ImGui_ImplVulkan_RenderDrawData`, close `mMainRenderPass`, end command buffer, submit, present, advance frame index.

---

### `beginFrame(glm::vec3 clearColor, ViewportRect gameViewport, int framebufferWidth, int framebufferHeight)`

1. `vkWaitForFences` on `mFrames[mCurrentFrame].inFlight`.
2. `vkAcquireNextImageKHR` → `mCurrentSwapchainImageIndex`. Handle `OUT_OF_DATE_KHR` / `SUBOPTIMAL_KHR` by calling `recreateSwapchain()`.
3. `vkResetFences`, `vkResetCommandBuffer`, `vkBeginCommandBuffer`.
4. Store `clearColor`, `gameViewport`, `framebufferWidth`, `framebufferHeight` as members for use in `beginMainPass` / `endFrame`.
5. Set `mActiveCommandBuffer`.

---

### `beginMainPass(ViewportRect gameViewport)`

1. Begin `mMainRenderPass` with `framebuffer = mSwapchainFramebuffers[mCurrentSwapchainImageIndex]`, `renderArea.extent = mSwapchainExtent`. Clear values: black `{0,0,0,1}` (color), `{1.0f, 0}` (depth). `LOAD_OP_CLEAR` paints the full framebuffer black.
2. Clear the game area to `mClearColor` (if not black) using `vkCmdClearAttachments` with a `VkClearRect` matching the game viewport.
3. Set dynamic viewport. **Y-flip**: Vulkan's Y origin is top-left; `ViewportRect.y` is from bottom (OpenGL convention):
   ```cpp
   vp.y      = float(mFramebufferHeight - gameViewport.y - gameViewport.height);
   vp.height = float(gameViewport.height);
   ```
4. Set scissor to the same rectangle.
5. Bind `mSpritePipeline`.

---

### `drawSprite(DMesh dmesh, DTexture dtexture, const SpriteDrawParams& params)`

1. Lookup `VulkanMesh` and `VulkanTexture`.
2. `vkCmdBindVertexBuffers`, `vkCmdBindIndexBuffer`.
3. `vkCmdBindDescriptorSets` with `tex.descriptorSet`.
4. `vkCmdPushConstants` with `packPushConstants(params)`.
5. `vkCmdDrawIndexed(cb, mesh.indexCount, 1, 0, 0, 0)`.

---

### `endFrame(GLFWwindow* window, ImDrawData* imguiData, int framebufferWidth, int framebufferHeight)`

1. Restore full-framebuffer viewport and scissor (for ImGui).
2. `ImGui_ImplVulkan_RenderDrawData(imguiData, mActiveCommandBuffer)`.
3. `vkCmdEndRenderPass`.
4. `vkEndCommandBuffer`.
5. Submit with `imageAvailable` wait semaphore (stage `COLOR_ATTACHMENT_OUTPUT`) and `renderFinished` signal semaphore + `inFlight` fence.
6. `vkQueuePresentKHR` with `renderFinished` wait semaphore. Handle `OUT_OF_DATE_KHR`.
7. `mCurrentFrame = (mCurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT`.

---

### `takeScreenshot(ViewportRect gameViewport, glm::ivec2 gameSize) const`

1. `vkDeviceWaitIdle` (declare members accessed here as `mutable`).
2. Create host-visible staging buffer of size `gameSize.x * gameSize.y * 4`.
3. Single-use command buffer:
   a. Transition current swapchain image `PRESENT_SRC_KHR → TRANSFER_SRC_OPTIMAL`.
   b. `vkCmdBlitImage` from swapchain (source region = game viewport rect) to an intermediate `R8G8B8A8_UNORM` image of `gameSize`. **Swap Y** (`srcOffsets[0].y = viewport.y + viewport.height`, `srcOffsets[1].y = viewport.y`) to achieve top-to-bottom row order.
   c. Transition intermediate image to `TRANSFER_SRC_OPTIMAL`.
   d. `vkCmdCopyImageToBuffer` into staging buffer.
   e. Host-read barrier.
   f. Transition swapchain image back to `PRESENT_SRC_KHR`.
4. `vkQueueWaitIdle`.
5. Map staging memory, copy into `result.data`, unmap.
6. Destroy staging buffer and intermediate image.

**Note on swapchain format**: If `B8G8R8A8_UNORM` (common on Windows NVIDIA), blit to `R8G8B8A8_UNORM` handles channel swap automatically if `VK_FORMAT_FEATURE_BLIT_DST_BIT` is supported. Fallback: blit without conversion, then swap B↔R in CPU readback loop.

---

### `setTextureMinFilter` / `setTextureMagFilter`

`VkSampler` objects are immutable after creation. For each call:
1. Skip if `tex.isProxy` (RTT proxy sampler is not user-modifiable).
2. `vkFreeDescriptorSets` for old descriptor set. `vkDestroySampler` for old sampler.
3. Create new `VkSampler` with updated filter.
4. Allocate new `VkDescriptorSet`, update with existing `imageView` + new sampler.
5. Store back in `tex`.

---

## 6. Render Pass Design

### `mMainRenderPass` — swapchain output

| Attachment | Format | loadOp | storeOp | initialLayout | finalLayout |
|---|---|---|---|---|---|
| Color | `mSwapchainFormat` | `CLEAR` | `STORE` | `UNDEFINED` | `PRESENT_SRC_KHR` |
| Depth | `mDepthFormat` | `CLEAR` | `DONT_CARE` | `UNDEFINED` | `DEPTH_STENCIL_ATTACHMENT_OPTIMAL` |

Subpass dependency: external → subpass 0, `COLOR_ATTACHMENT_OUTPUT | EARLY_FRAGMENT_TESTS` stages.

### `mRttRenderPass` — off-screen output

| Attachment | Format | loadOp | storeOp | initialLayout | finalLayout |
|---|---|---|---|---|---|
| Color | `R8G8B8A8_UNORM` | `CLEAR` | `STORE` | `COLOR_ATTACHMENT_OPTIMAL` | `COLOR_ATTACHMENT_OPTIMAL` |
| Depth | `mDepthFormat` | `CLEAR` | `DONT_CARE` | `UNDEFINED` | `DEPTH_STENCIL_ATTACHMENT_OPTIMAL` |

Image layout transitions `SHADER_READ_ONLY ↔ COLOR_ATTACHMENT` are handled by explicit pipeline barriers in `beginRttPass` / `endRttPass`, not embedded in the render pass.

The two passes **cannot be shared**: formats differ and `finalLayout` of the main pass is `PRESENT_SRC_KHR`.

---

## 7. RTT Pipeline Barriers

### `beginRttPass` — before `vkCmdBeginRenderPass`

```
srcStageMask  = FRAGMENT_SHADER
dstStageMask  = COLOR_ATTACHMENT_OUTPUT
srcAccessMask = SHADER_READ
dstAccessMask = COLOR_ATTACHMENT_WRITE
oldLayout     = SHADER_READ_ONLY_OPTIMAL
newLayout     = COLOR_ATTACHMENT_OPTIMAL
```

### `endRttPass` — after `vkCmdEndRenderPass`

```
srcStageMask  = COLOR_ATTACHMENT_OUTPUT
dstStageMask  = FRAGMENT_SHADER
srcAccessMask = COLOR_ATTACHMENT_WRITE
dstAccessMask = SHADER_READ
oldLayout     = COLOR_ATTACHMENT_OPTIMAL
newLayout     = SHADER_READ_ONLY_OPTIMAL
```

---

## 8. Double-Buffered Frame Loop

```
Frame slot N: mFrames[mCurrentFrame]

beginFrame:
  vkWaitForFences(inFlight[N])       ← ensure slot N's previous GPU work is done
  vkAcquireNextImageKHR              ← wait for swapchain image
  vkResetFences(inFlight[N])
  vkResetCommandBuffer / Begin

  ... RTT passes, main pass, draw calls, ImGui ...

endFrame:
  vkEndCommandBuffer
  vkQueueSubmit:
    wait   = imageAvailable[N]  (GPU waits for swapchain image to be writable)
    signal = renderFinished[N]  (GPU signals when color writes are done)
    fence  = inFlight[N]        (CPU detects completion for next reuse of slot N)
  vkQueuePresentKHR:
    wait   = renderFinished[N]

  mCurrentFrame = (mCurrentFrame + 1) % 2
```

No per-frame descriptor set duplication is needed — sprite data is in push constants, texture descriptors are persistent per-texture allocations.

---

## 9. Descriptor Set Strategy

**One `VkDescriptorSet` per `VulkanTexture`** (combined image sampler, set 0).

Rationale:
- Core Vulkan 1.0 — no extensions required.
- Matches the OpenGL model where each texture is independently bound.
- Pool size of 1024 is generous for a pixel-art renderer.
- `VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT` allows individual set freeing in `freeTexture` / `setTextureMinFilter`.

Use a **separate** `mImguiDescriptorPool` for ImGui to avoid pool interference.

---

## 10. `canvas_impl.cpp` Changes

Guard the GLFW window hints for OpenGL vs Vulkan:

```cpp
// Before glfwCreateWindow:
#if defined(NOTHOFAGUS_BACKEND_VULKAN)
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif

// After glfwCreateWindow:
#if !defined(NOTHOFAGUS_BACKEND_VULKAN)
    glfwMakeContextCurrent(mWindow->glfwWindow);
#endif
```

`glfwMakeContextCurrent` on a `GLFW_NO_API` window produces a GLFW error — suppress it explicitly rather than relying on it being a no-op.

---

## 11. `CMakeLists.txt` — Full Vulkan Block

```cmake
if(NOTHOFAGUS_BACKEND_VULKAN)
    find_package(Vulkan REQUIRED)

    add_subdirectory("third_party/vk-bootstrap")

    # VMA: header-only interface target
    add_library(VulkanMemoryAllocator INTERFACE)
    target_include_directories(VulkanMemoryAllocator INTERFACE
        "third_party/VulkanMemoryAllocator/include")

    target_compile_definitions(nothofagus PRIVATE NOTHOFAGUS_BACKEND_VULKAN)

    target_sources(nothofagus PRIVATE
        "source/vulkan_backend.cpp"
        "source/vulkan_mesh.cpp"
        "source/vulkan_texture.cpp"
        "source/vulkan_render_target.cpp"
        "source/vma_impl.cpp"             # #define VMA_IMPLEMENTATION
    )

    # SPIR-V compilation
    find_program(GLSLC glslc REQUIRED)
    set(SHADER_VERT_SRC "${CMAKE_CURRENT_SOURCE_DIR}/source/shaders/sprite.vert")
    set(SHADER_FRAG_SRC "${CMAKE_CURRENT_SOURCE_DIR}/source/shaders/sprite.frag")
    set(SHADER_VERT_SPV "${CMAKE_CURRENT_BINARY_DIR}/sprite.vert.spv")
    set(SHADER_FRAG_SPV "${CMAKE_CURRENT_BINARY_DIR}/sprite.frag.spv")
    add_custom_command(OUTPUT "${SHADER_VERT_SPV}"
        COMMAND "${GLSLC}" "${SHADER_VERT_SRC}" -o "${SHADER_VERT_SPV}"
        DEPENDS "${SHADER_VERT_SRC}" VERBATIM)
    add_custom_command(OUTPUT "${SHADER_FRAG_SPV}"
        COMMAND "${GLSLC}" "${SHADER_FRAG_SRC}" -o "${SHADER_FRAG_SPV}"
        DEPENDS "${SHADER_FRAG_SRC}" VERBATIM)
    add_custom_target(spirv_shaders DEPENDS "${SHADER_VERT_SPV}" "${SHADER_FRAG_SPV}")
    add_dependencies(nothofagus spirv_shaders)
    target_compile_definitions(nothofagus PRIVATE
        NOTHOFAGUS_SPRITE_VERT_SPV="${SHADER_VERT_SPV}"
        NOTHOFAGUS_SPRITE_FRAG_SPV="${SHADER_FRAG_SPV}")

    target_link_libraries(nothofagus PRIVATE
        Vulkan::Vulkan
        vk-bootstrap::vk-bootstrap
        VulkanMemoryAllocator)

    target_include_directories(nothofagus PRIVATE
        "third_party/VulkanMemoryAllocator/include")
else()
    # OpenGL backend sources (existing)
    ...
endif()
```

`source/vma_impl.cpp`:
```cpp
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
```

### `third_party/imgui_cmake/CMakeLists.txt`

Add a Vulkan backend option (set by the root CMakeLists before `add_subdirectory`):

```cmake
# imgui_cmake/CMakeLists.txt
option(IMGUI_VULKAN_BACKEND "Build ImGui Vulkan backend" OFF)
if(IMGUI_VULKAN_BACKEND)
    target_sources(imgui PRIVATE "${IMGUI_PATH}/backends/imgui_impl_vulkan.cpp")
    find_package(Vulkan REQUIRED)
    target_link_libraries(imgui PUBLIC Vulkan::Vulkan)
endif()
```

In the root `CMakeLists.txt`, before `add_subdirectory("third_party/imgui_cmake")`:
```cmake
if(NOTHOFAGUS_BACKEND_VULKAN)
    set(IMGUI_VULKAN_BACKEND ON CACHE BOOL "" FORCE)
endif()
```

---

## 12. New `CMakePresets.json` Entries

```json
{
    "name": "vulkan-base",
    "hidden": true,
    "inherits": "base",
    "cacheVariables": { "NOTHOFAGUS_BACKEND_VULKAN": "ON" }
},
{
    "name": "ninja-vulkan-debug",
    "description": "Ninja Debug — Vulkan backend (validation layers enabled)",
    "inherits": "vulkan-base",
    "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug" }
},
{
    "name": "ninja-vulkan-release",
    "description": "Ninja Release — Vulkan backend",
    "inherits": "vulkan-base",
    "cacheVariables": { "CMAKE_BUILD_TYPE": "Release" }
},
{
    "name": "ninja-vulkan-debug-examples",
    "description": "Ninja Debug Vulkan + examples",
    "inherits": "ninja-vulkan-debug",
    "cacheVariables": { "NOTHOFAGUS_BUILD_EXAMPLES": "ON" }
},
{
    "name": "ninja-vulkan-release-examples",
    "description": "Ninja Release Vulkan + examples",
    "inherits": "ninja-vulkan-release",
    "cacheVariables": { "NOTHOFAGUS_BUILD_EXAMPLES": "ON" }
},
{
    "name": "vs-vulkan-debug",
    "description": "Visual Studio 2022 Debug — Vulkan backend",
    "inherits": "vulkan-base",
    "generator": "Visual Studio 17 2022",
    "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug" }
}
```

---

## 13. New Source Files Summary

```
source/
    vulkan_backend.h            — VulkanBackend class, SpritePushConstants, FrameData,
                                  static_assert(RenderBackend<VulkanBackend>)
    vulkan_backend.cpp          — initialize, shutdown, beginFrame, imguiNewFrame,
                                  beginMainPass, endFrame, takeScreenshot
    vulkan_mesh.h               — VulkanMesh struct
    vulkan_mesh.cpp             — uploadMesh, freeMesh
    vulkan_texture.h            — VulkanTexture struct
    vulkan_texture.cpp          — uploadTexture, freeTexture, setTextureMin/MagFilter
    vulkan_render_target.h      — VulkanRenderTarget struct
    vulkan_render_target.cpp    — createRenderTarget, getRenderTargetTexture, freeRenderTarget,
                                  beginRttPass, endRttPass
    vma_impl.cpp                — #define VMA_IMPLEMENTATION; #include <vk_mem_alloc.h>
    shaders/
        sprite.vert
        sprite.frag
```

---

## 14. Verification Plan

| Step | What to test | Pass criterion |
|---|---|---|
| 1 | Compile `ninja-vulkan-debug` | Zero errors; `static_assert(RenderBackend<VulkanBackend>)` passes |
| 2 | Blank window | No validation errors; window shows correct clear color; clean shutdown |
| 3 | `hello_nothofagus` | Sprite renders at correct position with correct colors |
| 4 | Screenshot pixel comparison | Save screenshot from both backends for same deterministic frame; diff ≤ ±1 per channel |
| 5 | `hello_tint` | Tint and opacity match OpenGL output |
| 6 | `hello_render_to_texture` / `hello_nested_render_targets` | RTT output and nested RTT render correctly |
| 7 | Window resize | Letterbox/pillarbox bands appear; content undistorted |
| 8 | ImGui (`mStats = true`) | Stats overlay renders and responds to mouse |
| 9 | Animation layers | `hello_animation` cycles correctly via `layerIndex` push constant |
| 10 | Validation clean exit | Zero validation errors throughout steps 1–9, especially during `freeRenderTarget` and swapchain resize |
