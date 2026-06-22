#pragma once

#include "visual.h"
#include "imgui_image_size.h"          // ImguiImageSize / ImguiImageFit
#include "imgui_image_id.h"           // ImguiImageId
#include "render_snapshot.h"          // RttPass / DrawItem
#include "texture_id.h"
#include "mesh.h"                      // MeshId
#include "render_target.h"            // RenderTargetId
#include "ref_count.h"                // RefCount
#include "backends/render_backend_select.h"  // ActiveBackend

#include <glm/glm.hpp>
#include <map>
#include <optional>
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
 * Sim/render split aware: the sim side bakes a stable handle into the ImGui draw
 * list; all GPU work (RTT creation/draw, flat-2D handle creation) happens on the
 * render side, driven by ids.
 *
 * Two entry populations share one rendering path:
 *  - **Lazy** entries back the convenience `imguiVisual(...)` call. They are keyed by
 *    content + size, reference-counted by frame use, and garbage-collected a few frames
 *    after they stop being drawn. A genuinely first-seen visual is blank for one frame
 *    (the warm-up: the handle is created render-side this frame and read sim-side next).
 *  - **Registered** entries back `registerImguiImage(...)`. They have an explicit
 *    lifetime (until `unregisterImguiImage`), a fixed size spec, and a stable handle.
 *    Registration allocates the internal RTT immediately and the render side populates
 *    its handle on the next tick, so any draw that happens after registration is
 *    warm-up-free. They are never garbage-collected and never re-keyed on size change.
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

    // --- Registration (general ImGui image handles) ------------------------------------
    /// Register `visual` at a fixed `sizeSpec`. Returns immediately with a stable id; the
    /// internal RTT is allocated now and its ImGui handle becomes ready on the next render
    /// tick. The RTT/handle persist until `unregisterImage`.
    ImguiImageId registerImage(const Visual& visual, const ImguiImageSize::Spec& sizeSpec, float contentScale);
    /// Refresh a registered image's source appearance (layer / opacity / mesh / texture)
    /// without changing the id. Re-rasterizes next tick; recreates the RTT only if the
    /// physical size changed (which re-warms the handle).
    void updateImage(ImguiImageId id, const Visual& visual, float contentScale);
    /// Free a registered image's RTT, handle, and resource pins.
    void unregisterImage(ImguiImageId id);
    /// Sim-side: draw a registered image in the current ImGui window. `drawSize` (logical
    /// px) overrides the registered display size for this draw only (a GPU downscale of the
    /// fixed-resolution handle — used by markdown fit-to-width). Reserves layout while the
    /// handle is not yet ready or the visual is invisible.
    void drawImage(ImguiImageId id, std::optional<glm::vec2> drawSize);
    /// The registered image's ImGui-bindable `ImTextureID`, or 0 if unknown / not yet ready.
    std::uint64_t handleOf(ImguiImageId id) const;
    /// The registered image's resolved logical display size, or {0,0} if unknown.
    glm::vec2 sizeOf(ImguiImageId id) const;
    /// Whether the registered image's handle has been created (no warm-up remaining).
    bool isReady(ImguiImageId id) const;

    /// Sim-side: append this frame's internal RTT passes (lazy entries drawn this frame +
    /// registered entries that are displayed, dirty, or not-yet-populated) onto the
    /// snapshot's RTT pass list, after the user-scheduled RTT passes.
    void appendInternalPasses(std::vector<RttPass>& out);

    /// Render-side: after the snapshot's RTT passes have drawn the internal targets,
    /// refresh each flat-2D and (lazily) create its ImGui handle; then garbage-collect
    /// lazy entries unused for a while.
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

        // Registered-entry fields (unused for lazy entries).
        glm::vec2          displaySize{1.0f};               ///< logical points handed to ImGui.
        float              opacity = 1.0f;                  ///< source visual opacity.
        bool               visible = true;                  ///< source visual visibility.
        ImguiImageSize::Spec spec{ImguiImageSize::Natural{}}; ///< fixed size spec (for re-resolve).
        bool               everRendered = false;            ///< RTT populated at least once.
        bool               dirty = false;                   ///< force a re-render next tick.
        bool               renderedThisFrame = false;       ///< emitted a pass this frame.
    };

    // Geometry resolved from (Visual, size spec, scale), shared by the lazy and registered
    // paths — everything needed to allocate/key an RTT and draw the result.
    struct ResolvedGeom
    {
        TextureId  texId{0};
        MeshId     meshId{0};
        bool       ownedQuad = false;
        int        layer = 0;
        glm::ivec2 rttPhys{1, 1};
        bool       fitCentered = false;
        glm::mat3  transform{1.0f};
        glm::vec2  displaySize{1.0f};
    };

    MeshId ownedQuadFor(glm::ivec2 textureSize);
    /// Resolve mesh + rasterization geometry for a (visible) visual at the given spec/scale.
    ResolvedGeom resolveGeom(const Visual& visual, const ImguiImageSize::Spec& sizeSpec, float scale);
    /// Allocate the internal RTT + pin texture/mesh for `geom`, filling the GPU-side fields.
    void allocateEntry(Entry& entry, const ResolvedGeom& geom);
    /// Emit one internal RTT pass for `entry`.
    void pushPass(std::vector<RttPass>& out, const Entry& entry);
    /// Draw `handle` at `displaySize` in the current ImGui window, honoring the texture's
    /// magFilter and `opacity`. Caller guarantees `handle != 0`.
    void drawResolvedImage(std::uint64_t handle, glm::vec2 displaySize, TextureId texForFilter, float opacity);
    /// Best ready entry for the same sprite (same texture/mesh) as `want`, preferring
    /// matching layer, then size, then fit, then recency — to bridge the warm-up gap
    /// for both animation (layer change) and size-slider (size change). nullptr if none.
    Entry* findReadyFallback(const Key& key, const Entry& want);
    void   freeEntryGpu(Entry& entry);   ///< render-side: free handle + RTT.
    void   unpinEntry(const Entry& entry);

    ActiveBackend& mBackend;
    AssetRegistry& mAssets;

    std::map<Key, Entry>                 mEntries;       ///< lazy entries (imguiVisual).
    std::map<std::size_t, Entry>         mRegistered;    ///< registered entries (ImguiImageId).
    std::size_t                          mNextImageId = 1;
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
