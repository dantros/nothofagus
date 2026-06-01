#pragma once

#include "texture.h"
#include "texture_mode.h"
#include "indexed_container.h"
#include "dtexture.h"
#include "backends/render_backend_select.h"
#include <optional>
#include <cstdint>
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
    std::optional<DTexture> dflatTextureOpt;    ///< GPU flat 2D (ImGui-bindable) rep — lazy, created by ensureFlatRep().
    glm::ivec2 mTextureSize{0, 0}; ///< Cached size — set at creation for both CPU and proxy entries.
    TextureSampleMode minFilter = TextureSampleMode::Nearest;
    TextureSampleMode magFilter = TextureSampleMode::Nearest;
    TextureMode mode = TextureMode::Direct; ///< CPU-side texture kind. Proxy entries (no CPU texture) keep Direct since they are RGBA color attachments.

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
        dflatTextureOpt    = std::nullopt;
    }

    /// Lazily create the flat (ImGui-bindable) GPU representation of this texture
    /// and return its bindable handle (an `ImTextureID` value), or 0 for a proxy
    /// texture (no CPU pixels). Static path only: uploads via
    /// `uploadTexture(..., TextureUploadMode::Flat, ...)` (palette-resolved RGBA,
    /// layer 0). Cached in `dflatTextureOpt`; freed by `freeGpuResources`.
    std::uint64_t ensureFlatRep(ActiveBackend& backend);

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
