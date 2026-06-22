#pragma once

#include "visual.h"
#include "imgui_image_size.h"          // ImguiImageSize / ImguiImageFit
#include "render_snapshot.h"          // RttPass / DrawItem
#include "texture_id.h"
#include "mesh.h"                      // MeshId
#include "render_target.h"            // RenderTargetId
#include "ref_count.h"                // RefCount
#include "backends/render_backend_select.h"  // ActiveBackend

#include <glm/glm.hpp>
#include <map>
#include <tuple>
#include <utility>
#include <vector>
#include <cstdint>

namespace Nothofagus
{

class AssetRegistry;

/**
 * @class ImguiImageManager
 * @brief Draws a Visual's appearance inside an ImGui window (`ImGui::Image`).
 *
 * The inverse direction of ImguiRttManager (which renders ImGui *into* a render
 * target): this samples an engine-rendered sprite *from* an ImGui window. Engine
 * textures are 2D arrays / palette-indexed, so the Visual is first rendered into
 * an internal render target (RGBA, via the normal sprite path — Direct / Indirect
 * / tile-map / animation all work), and that RTT's color attachment is exposed to
 * ImGui as a flat-2D handle (an `ImTextureID`).
 *
 * Sim/render split aware: `imguiVisual` runs on the sim side and bakes a stable
 * handle into the ImGui draw list; all GPU work (RTT creation/draw, flat-2D
 * handle creation) happens on the render side, driven by ids. The handle is
 * created render-side and read sim-side, so a freshly shown visual is blank for
 * one frame (Phase 1 warm-up).
 */
class ImguiImageManager
{
public:
    ImguiImageManager(ActiveBackend& backend, AssetRegistry& assets)
        : mBackend(backend), mAssets(assets) {}

    /// Sim-side: bump the per-frame clock. Call once at the start of the build phase.
    void beginFrame() { ++mFrameCounter; }

    /// Sim-side, user-facing: draw `visual` in the current ImGui window, sized per
    /// `sizeSpec` (logical px). `contentScale` is the DPI density to rasterize the internal
    /// render target at (the same value the font atlas uses). Call inside an ImGui frame.
    void imguiVisual(const Visual& visual, const ImguiImageSize::Spec& sizeSpec, float contentScale);

    /// Sim-side: append this frame's internal RTT passes (one per visual drawn) onto
    /// the snapshot's RTT pass list, after the user-scheduled RTT passes.
    void appendInternalPasses(std::vector<RttPass>& out);

    /// Render-side: after the snapshot's RTT passes have drawn the internal targets,
    /// refresh each flat-2D and (lazily) create its ImGui handle; then garbage-collect
    /// entries unused for a while.
    void resolveAndGarbageCollect();

    /// Teardown: free every handle + internal RTT + resource pin. Backend and the
    /// main ImGui context must still be alive (called from ~Canvas before shutdown).
    void releaseAll();

private:
    // Identity of a drawn image: pixels (textureId, meshId, layer) + the rasterized
    // physical size + whether the content fills or is fit-centered (so the same visual at
    // different sizes / fit modes gets distinct render targets).
    using Key = std::tuple<std::size_t, std::size_t, std::size_t, int, int, int>;

    struct Entry
    {
        RenderTargetId rt{0};
        TextureId      texture{0};
        MeshId         mesh{0};
        int            layer = 0;
        glm::ivec2     rttSize{1, 1};       ///< physical px the target is rasterized at.
        glm::mat3      transform{1.0f};     ///< maps the mesh AABB into the RTT (pre rttNdc).
        std::uint64_t  handle = 0;          ///< ImTextureID; 0 until created render-side.
        std::uint64_t  lastUsedFrame = 0;
        bool           ownedQuad = false;   ///< mesh is a manager-synthesized quad.
    };

    MeshId ownedQuadFor(glm::ivec2 textureSize);
    /// Best ready entry for the same sprite (same texture/mesh) as `want`, preferring
    /// matching layer, then size, then fit, then recency — to bridge the warm-up gap
    /// for both animation (layer change) and size-slider (size change). nullptr if none.
    Entry* findReadyFallback(const Key& key, const Entry& want);
    void   freeEntryGpu(Entry& entry);   ///< render-side: free handle + RTT.
    void   unpinEntry(const Entry& entry);

    ActiveBackend& mBackend;
    AssetRegistry& mAssets;

    std::map<Key, Entry>                 mEntries;
    std::map<std::pair<int, int>, MeshId> mOwnedQuads;   ///< quad mesh per texture size (shared).

    // Reference counts for resource pins so a texture/mesh shared by several entries
    // is retained on the AssetRegistry once and released only when the last entry
    // referencing it is retired.
    RefCount<TextureId> mTexturePins;
    RefCount<MeshId>    mMeshPins;

    std::uint64_t mFrameCounter = 0;
    static constexpr std::uint64_t kRetireAfterFrames = 8;
};

} // namespace Nothofagus
