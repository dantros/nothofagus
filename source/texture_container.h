#pragma once

#include "texture.h"
#include "texture_mode.h"
#include "indexed_container.h"
#include "dtexture.h"
#include "backends/render_backend_select.h"
#include <optional>
#include <glm/glm.hpp>

namespace Nothofagus
{

struct TexturePack
{
    // nullopt for GPU-proxy entries (render target color attachments).
    // CPU-owned textures (IndirectTexture / DirectTexture) always have a value.
    std::optional<Texture> texture;
    std::optional<DTexture> dtextureOpt;
    std::optional<DTexture> dpaletteTextureOpt; ///< GPU palette texture (only for indirect textures).
    std::optional<DTexture> dmapTextureOpt;     ///< GPU map texture (only for tile-map textures).
    glm::ivec2 mTextureSize{0, 0}; ///< Cached size — set at creation for both CPU and proxy entries.
    TextureSampleMode minFilter = TextureSampleMode::Nearest;
    TextureSampleMode magFilter = TextureSampleMode::Nearest;
    TextureMode mode = TextureMode::Direct; ///< CPU-side texture kind. Proxy entries (no CPU texture) keep Direct since they are RGBA color attachments.

    /// Pure-CPU deferral flags so sim-thread mutations never touch the GPU directly
    /// (the GPU work is consumed by `syncToGpu` on the render thread). `mContentDirty`
    /// forces a full free + re-upload (the `markTextureAsDirty` signal, e.g. after the
    /// caller mutated raw pixels); `mFilterDirty` re-applies min/mag filters to the
    /// live GPU texture (`setTextureMin|MagFilter`).
    bool mContentDirty = false;
    bool mFilterDirty = false;

    bool isProxy() const { return not texture.has_value(); }
    bool isDirty() const { return not dtextureOpt.has_value(); }

    /// Reset every GPU-side optional to nullopt. Does NOT touch the backend —
    /// the caller is responsible for freeing the underlying handles first.
    /// Use `freeGpuResources(backend)` when you want both at once.
    void clear()
    {
        dtextureOpt        = std::nullopt;
        dpaletteTextureOpt = std::nullopt;
        dmapTextureOpt     = std::nullopt;
    }

    /// Free every backend handle this pack owns (palette + map + main texture),
    /// then reset the optionals. The main texture is only freed for non-proxy
    /// entries — proxy textures (render target color attachments) are owned by
    /// their RenderTargetPack and freed through `freeRenderTarget`.
    void freeGpuResources(ActiveBackend& backend);

    /// Per-frame GPU sync: lazy-upload on first use, then re-upload / patch
    /// whichever subset of atlas / map / palette the CPU side has marked dirty.
    /// Proxy entries (render-target color attachments) are skipped here — the
    /// owning RenderTargetPack manages their GPU side via its own `syncToGpu`.
    void syncToGpu(ActiveBackend& backend);
};

using TextureContainer = IndexedContainer<TexturePack>;

}
