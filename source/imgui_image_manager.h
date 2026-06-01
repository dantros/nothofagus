#pragma once

#include "bellota.h"            // TextureId
#include "backends/render_backend_select.h"  // ActiveBackend
#include <glm/glm.hpp>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

namespace Nothofagus
{

class AssetRegistry;

/**
 * @brief Bridges engine textures to ImGui-bindable images (`ImTextureID`) for
 *        `ImGui::Image` — the basis of inline markdown images.
 *
 * The ImGui-bindable artifact is the texture's **flat representation**: a plain
 * 2D RGBA GPU texture that lives inside the source `TexturePack`
 * (`ensureFlatRep`), uploaded via the native texture path — not a bespoke
 * parallel resource. `handle(texId)` resolves it; `imguiImageHandle` on the
 * canvas delegates here.
 *
 * Lifetime: a texture shown only as an ImGui image has no bellota, so the
 * per-frame texture GC would drop it (and its flat rep). The manager **pins**
 * such sources while shown and **unpins** them when they stop being requested
 * (touch-GC): `handle()` stamps the current frame; `endFrame()` unpins any
 * source not requested this frame, so it (and its flat rep) auto-free once the
 * window closes / markdown drops it — unless a real bellota still references it.
 *
 * Only static single-frame sources (Direct, single-layer non-tilemap Indirect)
 * are supported here; animated / tile-map textures are declined (Phase 3 adds a
 * render-target-backed flat rep for them).
 */
class ImguiImageManager
{
public:
    ImguiImageManager(ActiveBackend& backend, AssetRegistry& assets)
        : mBackend(backend), mAssets(assets)
    {
    }

    /// Resolve a `TextureId` to an ImGui-bindable handle (an `ImTextureID`
    /// value), lazily creating its flat rep + pinning the source. Returns 0 for
    /// an unknown id, a render-target proxy, or an unsupported (dynamic) source.
    std::uint64_t handle(TextureId textureId);

    /// Full pixel extent of the texture, or {0, 0} if unknown.
    glm::ivec2 size(TextureId textureId) const;

    /// Unpin any pinned source not requested this frame (auto-free on hide), then
    /// advance the frame counter. Call once per frame from the frame loop.
    void endFrame();

private:
    ActiveBackend& mBackend;
    AssetRegistry& mAssets;

    std::unordered_map<std::size_t, std::uint64_t> mPinned;  ///< pinned source -> last frame requested.
    std::unordered_set<std::size_t> mDeclinedWarned;         ///< dynamic sources warned about (once each).
    std::uint64_t mFrameCounter = 0;
};

}
