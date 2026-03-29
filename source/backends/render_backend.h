#pragma once

#include "dmesh.h"
#include "dtexture.h"
#include "drender_target.h"
#include "texture.h"       // Texture (CPU-side), TextureSampleMode
#include "mesh.h"          // Mesh (CPU-side)
#include "canvas.h"        // ViewportRect
#include <glm/glm.hpp>
#include <concepts>
#include <cstdint>
#include <vector>

struct ImDrawData;

namespace Nothofagus
{

struct SpriteDrawParams
{
    glm::mat3 transform;        ///< worldTransform * bellota.transform().toMat3()
    int       layerIndex;
    glm::vec3 tintColor;
    float     tintIntensity;
    float     opacity;
};

/// Pixel buffer returned by takeScreenshot(). RGBA, top-to-bottom row order.
struct ScreenshotPixels
{
    std::vector<std::uint8_t> data;
    int width, height;
};

template <typename Backend>
concept RenderBackend = requires(
    Backend& backend,
    void* nativeWindowHandle,
    glm::ivec2 canvasSize,
    const Texture& texture,
    TextureSampleMode samplerMode,
    const Mesh& mesh,
    glm::ivec2 targetSize,
    DMesh dmesh,
    DTexture dtexture,
    DRenderTarget renderTarget,
    glm::vec3 clearColor3,
    glm::vec4 clearColor4,
    ViewportRect viewport,
    int framebufferWidth,
    int framebufferHeight,
    const SpriteDrawParams& params,
    ImDrawData* imguiData)
{
    // Lifecycle
    { backend.initialize(nativeWindowHandle, canvasSize) } -> std::same_as<void>;
    { backend.shutdown()                                  } -> std::same_as<void>;
    { backend.initImGuiRenderer()                         } -> std::same_as<void>;

    // GPU resource management
    { backend.uploadTexture(texture, samplerMode, samplerMode) } -> std::same_as<DTexture>;
    { backend.freeTexture(dtexture)                            } -> std::same_as<void>;
    { backend.uploadMesh(mesh)                                 } -> std::same_as<DMesh>;
    { backend.freeMesh(dmesh)                                  } -> std::same_as<void>;
    { backend.createRenderTarget(targetSize)                   } -> std::same_as<DRenderTarget>;
    { backend.getRenderTargetTexture(renderTarget)             } -> std::same_as<DTexture>;
    // proxyTexture is the DTexture returned by getRenderTargetTexture — backend must
    // remove it from its internal texture map without calling glDeleteTextures on it
    // (the GL handle is owned by the render target's color attachment and freed by freeRenderTarget).
    { backend.freeRenderTarget(renderTarget, dtexture)         } -> std::same_as<void>;

    // Per-frame rendering
    { backend.beginFrame(clearColor3, viewport, framebufferWidth, framebufferHeight) } -> std::same_as<void>;
    { backend.imguiNewFrame()                                  } -> std::same_as<void>;
    { backend.beginRttPass(renderTarget, clearColor4)          } -> std::same_as<void>;
    { backend.endRttPass()                                     } -> std::same_as<void>;
    { backend.beginMainPass(viewport)                          } -> std::same_as<void>;
    { backend.drawSprite(dmesh, dtexture, params)              } -> std::same_as<void>;
    // Renders ImGui draw data. Buffer swap is handled by the window backend.
    { backend.endFrame(imguiData, framebufferWidth, framebufferHeight) } -> std::same_as<void>;

    // Screenshot: reads from the front buffer, returns RGBA pixels top-to-bottom.
    { backend.takeScreenshot(viewport, canvasSize)             } -> std::same_as<ScreenshotPixels>;
};

} // namespace Nothofagus
