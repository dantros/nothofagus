#pragma once

#include "canvas.h"
#include "dense_land.h"
#include "sparse_land.h"
#include "explorer.h"
#include "explorer_manager.h"
#include "bellota_container.h"   // for BellotaPack in mSortedBellotaPacks
#include "aa_box.h"
#include "backends/render_backend_select.h"
#include <vector>
#include <utility>
#include <cstddef>
#include <optional>
#include <memory>

struct ImGuiStyle; // global-scope (Dear ImGui); held by unique_ptr to keep imgui.h out of this header.

namespace Nothofagus
{

using DenseLandExplorerManager   = ExplorerManager<DenseLand>;
using SparseLandExplorerManager = ExplorerManager<SparseLand>;

// Explicit instantiations live in explorer_manager.cpp. Declared here (next to
// where DenseLand/SparseLand are already in scope) instead of inside
// explorer_manager.h so that header stays free of any concrete-backend
// references. The aliases above can't be used in this form — explicit
// instantiation requires a template-id, not a typedef-name.
extern template class ExplorerManager<DenseLand>;
extern template class ExplorerManager<SparseLand>;

// Forward decls — FrameRunner only takes these by reference (in run/tick), so
// the full headers don't need to be visible here. frame_runner.h is an
// internal header (never reached by the public canvas.h), so these names
// at namespace scope stay out of the public-API surface.
class AssetRegistry;
class ImguiRttManager;

/**
 * @class FrameRunner
 * @brief Internal owner of the GPU backend, window/input backend, and the
 * per-frame render loop. Asset CRUD (textures/meshes/bellotas/render targets)
 * and the canvas-wide ImGui font / RTT-context manager live on `Canvas`
 * itself; FrameRunner exposes the GPU backend and the small predicates the
 * cross-cutting gates need.
 *
 * Construction order in `Canvas`'s ctor is `mFrameRunner` first (this builds
 * `mBackend`), then `mAssets(mFrameRunner->backend())`, then
 * `mImguiRtt(mFrameRunner->backend(), mAssets->renderTargets(), …)`.
 */
class FrameRunner
{
public:

    FrameRunner(
        const ScreenSize& screenSize,
        const std::string& title,
        const glm::vec3 clearColor,
        const unsigned int pixelSize,
        bool headless = false
    );

    /// Destructor to clean up resources and terminate the window backend.
    /// Caller (Canvas) must drain `mImguiRtt` and `mAssets` GPU resources
    /// BEFORE destroying FrameRunner, because `~FrameRunner` shuts the backend
    /// down.
    ~FrameRunner();

    // ----- Backend access (internal, never reaches the public canvas.h surface) -----
    ActiveBackend&       backend()       noexcept { return mBackend; }
    const ActiveBackend& backend() const noexcept { return mBackend; }
    /// Effective content scale (DPI factor) used to scale the main standard-UI
    /// ImGui context: the override if set, else the window backend's OS scale.
    /// Only valid after construction.
    float                contentScale() const;
    /// Override the OS content scale (e.g. for accessibility/zoom or deterministic
    /// tests). Pass std::nullopt to revert to the backend-reported value.
    void                 setContentScaleOverride(std::optional<float> scale)                 { mContentScaleOverride = scale; }
    std::optional<float> contentScaleOverride() const                                        { return mContentScaleOverride; }

    // ----- Cross-cutting predicates used by Canvas's remove-gates -----
    bool isExplorerManagedBellota(std::size_t bellotaId) const
    {
        return mDenseLandManager.isExplorerManagedBellota(bellotaId)
            || mSparseLandManager.isExplorerManagedBellota(bellotaId);
    }
    bool isExplorerManagedTexture(std::size_t textureId) const
    {
        return mDenseLandManager.isExplorerManagedTexture(textureId)
            || mSparseLandManager.isExplorerManagedTexture(textureId);
    }

    // ----- Window / display (depend on pimpl-hidden Window) -----
    std::size_t getCurrentMonitor() const;
    bool isFullscreen() const;
    void setFullScreenOnMonitor(std::size_t monitor = 0);
    AABox getWindowAABox() const;
    void setWindowed();
    void setWindowTitle(const std::string& title);
    ScreenSize windowSize() const;
    void close();

    // ----- Canvas state -----
    const ScreenSize& screenSize() const                                                    { return mScreenSize; }
    void setScreenSize(const ScreenSize& screenSize)                                        { mScreenSize = screenSize; }
    void setClearColor(glm::vec3 clearColor)                                                { mClearColor = clearColor; }
    ViewportRect gameViewport() const                                                       { return mGameViewport; }
    bool& stats()                                                                           { return mStats; }
    const bool& stats() const                                                               { return mStats; }
    void setAutoRemoveUnusedTextures(bool enabled)                                          { mAutoTextureGC = enabled; }
    void setAutoRemoveUnusedMeshes(bool enabled)                                            { mAutoMeshGC = enabled; }

    // ----- DenseLands (DenseLandExplorerManager stays with FrameRunner) -----
    DenseLandId addDenseLand(DenseLand denseLand)                                                   { return mDenseLandManager.add(std::move(denseLand)); }
    void removeDenseLand(DenseLandId denseLandId)                                                 { mDenseLandManager.remove(denseLandId); }
    DenseLand& denseLand(DenseLandId denseLandId)                                                   { return mDenseLandManager.get(denseLandId); }
    const DenseLand& denseLand(DenseLandId denseLandId) const                                       { return mDenseLandManager.get(denseLandId); }
    DenseLandExplorerId addDenseLandExplorer(DenseLandExplorer explorer, Canvas& canvas)          { return mDenseLandManager.addExplorer(explorer, canvas); }
    void removeDenseLandExplorer(DenseLandExplorerId explorerId, Canvas& canvas)                { mDenseLandManager.removeExplorer(explorerId, canvas); }
    DenseLandExplorer& denseLandExplorer(DenseLandExplorerId explorerId)                          { return mDenseLandManager.getExplorer(explorerId); }
    const DenseLandExplorer& denseLandExplorer(DenseLandExplorerId explorerId) const              { return mDenseLandManager.getExplorer(explorerId); }

    // ----- SparseLands (SparseLandExplorerManager stays with FrameRunner) -----
    SparseLandId addSparseLand(SparseLand sparseLand)                                                   { return mSparseLandManager.add(std::move(sparseLand)); }
    void removeSparseLand(SparseLandId sparseLandId)                                                   { mSparseLandManager.remove(sparseLandId); }
    SparseLand& sparseLand(SparseLandId sparseLandId)                                                   { return mSparseLandManager.get(sparseLandId); }
    const SparseLand& sparseLand(SparseLandId sparseLandId) const                                       { return mSparseLandManager.get(sparseLandId); }
    SparseLandExplorerId addSparseLandExplorer(SparseLandExplorer explorer, Canvas& canvas)            { return mSparseLandManager.addExplorer(explorer, canvas); }
    void removeSparseLandExplorer(SparseLandExplorerId explorerId, Canvas& canvas)                    { mSparseLandManager.removeExplorer(explorerId, canvas); }
    SparseLandExplorer& sparseLandExplorer(SparseLandExplorerId explorerId)                            { return mSparseLandManager.getExplorer(explorerId); }
    const SparseLandExplorer& sparseLandExplorer(SparseLandExplorerId explorerId) const                { return mSparseLandManager.getExplorer(explorerId); }

    // ----- RTT pass scheduling (the queue lives here; consumed in runOneFrame) -----
    void renderTo(RenderTargetId renderTargetId, std::vector<BellotaId> bellotaIds)         { mPendingRttPasses.emplace_back(renderTargetId, std::move(bellotaIds)); }

    // ----- Lifecycle -----
    /// Runs the main loop. Threads through the Canvas-owned asset registry and
    /// ImGui RTT manager so runOneFrame doesn't need direct member access.
    void run(Canvas& canvas, AssetRegistry& assets, ImguiRttManager& imguiRtt,
             std::function<void(float deltaTime)> update, Controller& controller);

    /// Execute a single frame with a caller-supplied delta time (in milliseconds).
    void tick(Canvas& canvas, AssetRegistry& assets, ImguiRttManager& imguiRtt,
              float deltaTimeMS, std::function<void(float)> update, Controller& controller);

    /// Captures the last rendered frame visible to the user as a DirectTexture (RGBA).
    DirectTexture takeScreenshot() const;

private:
    void ensureSessionStarted(Controller& controller);
    void runOneFrame(Canvas& canvas, AssetRegistry& assets, ImguiRttManager& imguiRtt,
                     float deltaTimeMS, std::function<void(float)> update, Controller& controller);
    /// Apply the current effective content scale to the main ImGui context:
    /// FontScaleDpi (fonts, every frame) + ScaleAllSizes from the pristine base
    /// style (metrics, only when the scale changed). Main context must be current.
    void applyMainContextScale();

    ScreenSize mScreenSize; ///< The screen size of the canvas.
    std::string mTitle; ///< The title of the canvas window.
    glm::vec3 mClearColor; ///< The background color of the canvas.
    unsigned int mPixelSize; ///< The pixel size on the canvas.

    ActiveBackend mBackend; ///< GPU rendering backend (compile-time selected).

    DenseLandExplorerManager   mDenseLandManager;   ///< Dense land (huge dense-world) storage + per-frame explorer pool logic.
    SparseLandExplorerManager mSparseLandManager; ///< Sparse denseLand storage + per-frame explorer pool logic.

    /// RTT passes queued by renderTo() during the update callback, executed before the main render.
    std::vector<std::pair<RenderTargetId, std::vector<BellotaId>>> mPendingRttPasses;

    bool mStats; ///< Flag to indicate whether stats should be displayed.
    bool mHeadless{false}; ///< When true, the window is hidden (no visible UI).
    bool mSessionStarted{false}; ///< True after ensureSessionStarted() has been called.
    bool mAutoTextureGC{true}; ///< When true, unreferenced textures are removed each frame.
    bool mAutoMeshGC{true};    ///< When true, unreferenced meshes are removed each frame.
    std::vector<const BellotaPack*> mSortedBellotaPacks; ///< Reusable depth-sorted draw list.

    struct Window; ///< Forward declaration for window management.
    std::unique_ptr<Window> mWindow; ///< Pointer to the window object.

    AABox mLastWindowedAABox;
    ViewportRect mGameViewport; ///< Current letterboxed game viewport (set each frame in run()).

    std::optional<float> mContentScaleOverride; ///< When set, replaces the backend OS content scale.
    std::unique_ptr<ImGuiStyle> mBaseStyle;     ///< Pristine (scale-1) style sizes; reference for ScaleAllSizes.
    float mAppliedScale{1.0f};                   ///< Last scale applied to the main context's metrics.
};

}
