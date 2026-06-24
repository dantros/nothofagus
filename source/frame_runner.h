#pragma once

#include "canvas.h"
#include "dense_land.h"
#include "sparse_land.h"
#include "explorer.h"
#include "explorer_manager.h"
#include "bellota_container.h"
#include "render_snapshot.h"
#include "snapshot_buffers.h"
#include "performance_monitor.h"
#include "aa_box.h"
#include "backends/render_backend_select.h"
#include <vector>
#include <utility>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <memory>
#include <atomic>
#include <functional>
#include <mutex>
#include <chrono>

struct ImGuiStyle;   // global-scope (Dear ImGui); held by unique_ptr to keep imgui.h out of this header.
struct ImGuiContext; // global-scope; the sim-thread UI context (M3) is held as an opaque pointer.

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
class ImguiImageManager;

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
        bool headless = false,
        PresentMode presentMode = DEFAULT_PRESENT_MODE,
        std::optional<float> targetFps = std::nullopt
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
    // mScreenSize is atomic: on the threaded path the sim thread may setScreenSize()
    // from commit's update while the render thread reads it (viewport/letterbox).
    // Returns by value (a consistent 8-byte load); never a reference into the atomic.
    ScreenSize screenSize() const                                                           { return mScreenSize.load(std::memory_order_acquire); }
    void setScreenSize(const ScreenSize& screenSize)                                        { mScreenSize.store(screenSize, std::memory_order_release); }
    void setClearColor(glm::vec3 clearColor)                                                { mClearColor = clearColor; }
    /// Frame-rate cap for run(); a present value <= 0 is normalized to nullopt (unlimited).
    void setTargetFps(std::optional<float> targetFps)                                       { mTargetFps = (targetFps && *targetFps > 0.0f) ? targetFps : std::nullopt; }
    std::optional<float> targetFps() const                                                  { return mTargetFps; }
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
    /// ImGui RTT / image managers so runOneFrame doesn't need direct member access.
    void run(Canvas& canvas, AssetRegistry& assets, ImguiRttManager& imguiRtt,
             ImguiImageManager& imguiImages,
             std::function<void(float deltaTime)> update, Controller& controller);

    /// Execute a single frame with a caller-supplied delta time (in milliseconds).
    void tick(Canvas& canvas, AssetRegistry& assets, ImguiRttManager& imguiRtt,
              ImguiImageManager& imguiImages,
              float deltaTimeMS, std::function<void(float)> update, Controller& controller);

    /// Captures the last rendered frame visible to the user as a DirectTexture (RGBA).
    DirectTexture takeScreenshot() const;

    // ----- Threaded driver (M2 Phase A) -----
    // The app runs two threads: `commitFrame` on a sim thread and
    // `renderFrameThreaded` on the main thread. nothofagus spawns nothing.
    // The sim thread (commit's update) may mutate bellota values, create/destroy
    // bellotas/textures/meshes/render targets, run interactive ImGui (uiCallback),
    // use gamepad/keyboard/mouse, and drive Dense/Sparse land explorers — all
    // serialized against the renderer.

    /// Main thread: bind input, reset the close flag, mark the threaded session
    /// live, and remember the canvas so the threaded producer can drive explorers.
    void beginThreadedSession(Canvas& canvas, Controller& controller);

    /// Thread-safe: true until the window is closed. Read by the sim loop.
    bool threadedRunning() const { return mThreadedRunning.load(std::memory_order_acquire); }

    /// Thread-safe: whether the threaded ImGui UI captured the mouse / keyboard on
    /// the most recent commit (so host game logic can ignore that input).
    bool threadedWantsMouse() const { return mImguiWantsMouse.load(std::memory_order_acquire); }
    bool threadedWantsKeyboard() const { return mImguiWantsKeyboard.load(std::memory_order_acquire); }

    /// Sim thread: run the user `update` (game logic, lock-free), then — under the
    /// ImGui mutex — run `uiCallback` as an ImGui frame on the sim-UI context and
    /// clone its draw data, and project the scene into a free snapshot slot, then
    /// publish. `uiCallback` may be empty (no ImGui). No GL.
    void commitFrame(AssetRegistry& assets, float deltaTimeMS,
                     std::function<void(float)> update, std::function<void(float)> uiCallback);

    /// Sim thread (M5): like commitFrame above but, before the user update, feeds
    /// the given sim controller from the latest gamepad snapshot (no ImGui).
    void commitFrame(AssetRegistry& assets, float deltaTimeMS,
                     std::function<void(float)> update, Controller& simController);

    /// Main thread: acquire the latest published snapshot and render it (GPU
    /// upload + draw + present), poll window/input, and refresh the running flag.
    void renderFrameThreaded(AssetRegistry& assets, ImguiRttManager& imguiRtt, Controller& controller);

    /// Add a bellota. Safe both single-threaded (run/tick or setup — no lock) and
    /// from inside commit()'s update on the sim thread while a threaded session is
    /// live (takes the asset mutex). Returns the new id.
    BellotaId addBellota(AssetRegistry& assets, const Bellota& bellota);

    /// Remove a bellota. In a live threaded session this also queues any resources
    /// it orphaned for deferred GPU free on the render thread; single-threaded the
    /// orphans are GC'd by the next produce(Single) pass.
    void removeBellota(AssetRegistry& assets, BellotaId bellotaId);

    // ----- Mode-aware resource create/destroy -----
    // Each takes the asset mutex only while a threaded session is live (no lock
    // single-threaded). Adds + CPU-only mutators just forward (GPU upload is lazy
    // via syncToGpu). Removes defer the GPU free to the render thread when threaded
    // (enqueue + drainPendingFrees), or free immediately single-threaded.
    TextureId      addTexture(AssetRegistry& assets, const Texture& texture);
    void           removeTexture(AssetRegistry& assets, TextureId textureId);
    void           setTexture(AssetRegistry& assets, BellotaId bellotaId, TextureId textureId);
    void           markTextureAsDirty(AssetRegistry& assets, TextureId textureId);
    void           setTextureMinFilter(AssetRegistry& assets, TextureId textureId, TextureSampleMode mode);
    void           setTextureMagFilter(AssetRegistry& assets, TextureId textureId, TextureSampleMode mode);
    MeshId         addMesh(AssetRegistry& assets, const Mesh& mesh);
    MeshId         addMesh(AssetRegistry& assets, Mesh&& mesh);
    void           removeMesh(AssetRegistry& assets, MeshId meshId);
    void           setMesh(AssetRegistry& assets, BellotaId bellotaId, MeshId meshId);
    RenderTargetId addRenderTarget(AssetRegistry& assets, ScreenSize size);
    void           removeRenderTarget(AssetRegistry& assets, RenderTargetId renderTargetId);
    void           setRenderTargetClearColor(AssetRegistry& assets, RenderTargetId renderTargetId, glm::vec4 clearColor);

private:
    /// Returns a lock on the asset mutex while a threaded session is live, else an
    /// empty (unlocked) lock — the single-threaded fast path takes no mutex.
    std::unique_lock<std::recursive_mutex> lockAssetsIfThreaded();

    /// Selects which orchestration a unified producer/consumer runs. `Single` is
    /// the run()/tick() path (one thread; ImGui on the main context, live draw
    /// data, no locks); `Threaded` is the commit()/renderFrame() path (sim/render
    /// split; sim-UI context + cloned draw data, asset/ImGui mutexes). The two
    /// share their leaf work and differ only in the mode-gated arms.
    enum class FrameMode { Single, Threaded };

    /// Render thread (M3): snapshot the main context's processed ImGui input
    /// (mouse pos/buttons/wheel + display size/scale) into mThreadedImguiInput so
    /// the sim thread can feed it to the sim-UI context, making widgets interactive.
    void harvestImguiInput();

    /// Render thread (M5): snapshot the render controller's normalized gamepad
    /// state into mThreadedGamepadState (called after the window poll).
    void harvestGamepadInput(Controller& renderController);

    /// Sim thread (M5): replay the latest gamepad snapshot onto the sim controller
    /// (diff buttons/connection vs its current state, set axes, then dispatch the
    /// queued button edges). Called before the user update.
    void feedGamepadInput(Controller& simController);

    /// Render thread: snapshot the render controller's held keyboard/mouse state +
    /// per-frame scroll into mThreadedGameInputState (called after the window poll).
    void harvestGameInput(Controller& renderController);

    /// Sim thread: replay the latest keyboard/mouse snapshot onto the sim controller
    /// (diff key/button state vs its current state → press/release edges, set mouse
    /// position, forward scroll, then dispatch the queued edges). Before the update.
    void feedGameInput(Controller& simController);
    void ensureSessionStarted(Controller& controller);
    void runOneFrame(Canvas& canvas, AssetRegistry& assets, ImguiRttManager& imguiRtt,
                     ImguiImageManager& imguiImages,
                     float deltaTimeMS, std::function<void(float)> update, Controller& controller);

    /// Unified producer for both modes. `Single` (run/tick) polls input, opens the
    /// GPU + main ImGui frame, runs `update` (which issues main-context ImGui),
    /// updates explorers, stamps the commit seq + GCs unused resources, projects
    /// into the reused `mSnapshot`, and returns it. `Threaded` (commit) stamps the
    /// seq first, runs `update` lock-free, then under `mImguiMutex` runs `uiCallback`
    /// on the sim-UI context + clones its draw data into the triple-buffer write
    /// slot, projects, publishes, and returns that slot. `canvas`, `imguiRtt`,
    /// `imguiImages`, and `controller` are used only in `Single` (the sim thread has
    /// none) — pass `nullptr` in `Threaded`; `uiCallback` is empty in `Single`.
    const RenderSnapshot& produce(FrameMode mode, Canvas* canvas, AssetRegistry& assets,
                                  ImguiRttManager* imguiRtt, ImguiImageManager* imguiImages,
                                  float deltaTimeMS,
                                  std::function<void(float)> update,
                                  std::function<void(float)> uiCallback,
                                  Controller* controller);

    /// Unified consumer for both modes. `Single` (run/tick): the GPU frame + main
    /// ImGui frame were already opened by the producer, so this just runs the
    /// render core, the optional stats overlay, the live-ImGui render, and the
    /// swap — no locks. `Threaded` (renderFrame): the caller has already
    /// acquired/read the snapshot; this dispatches queued input, opens the GPU
    /// frame + an empty main ImGui frame (harvesting input for the sim), computes
    /// wall-clock dt, runs the render core under `mThreadedAssetMutex`, renders the
    /// sim's cloned draw data under `mImguiMutex`, swaps, and refreshes the running
    /// flag. The `deltaTimeMS` argument is used only in `Single`; `Threaded`
    /// recomputes it from the window clock. `imguiImages` is null in `Threaded`
    /// (imguiVisual is single-threaded only for now).
    void consume(FrameMode mode, AssetRegistry& assets, ImguiRttManager& imguiRtt,
                 ImguiImageManager* imguiImages,
                 const RenderSnapshot& snapshot, float deltaTimeMS, Controller& controller);

    /// The container-touching core of a rendered frame: deferred frees, GPU
    /// upload, RTT passes (including imguiVisual's internal RTTs, resolved +
    /// GC'd via `imguiImages` when non-null), and the main draw. Excludes the
    /// vsync swap and the ImGui render. On the threaded path the caller holds
    /// `mThreadedAssetMutex` around this; single-threaded there is no contention.
    void renderSnapshotContents(AssetRegistry& assets, ImguiRttManager& imguiRtt,
                                ImguiImageManager* imguiImages,
                                const RenderSnapshot& snapshot, float deltaTimeMS);

    /// Gather the queued RTT passes (`mPendingRttPasses`) into POD draw lists on
    /// `out` and clear the queue. CPU-only — GPU existence of each render target
    /// is checked later, on the render side.
    void buildRttPasses(AssetRegistry& assets, std::vector<RttPass>& out);

    /// Free every pending resource whose retire commit is no later than
    /// `lastRenderedSeq` (i.e. no in-flight snapshot still references it).
    void drainPendingFrees(AssetRegistry& assets, ImguiRttManager& imguiRtt, std::uint64_t lastRenderedSeq);
    /// Apply the current effective content scale to the main ImGui context:
    /// FontScaleDpi (fonts, every frame) + ScaleAllSizes from the pristine base
    /// style (metrics, only when the scale changed). Main context must be current.
    void applyMainContextScale();

    /// Open a Dear ImGui frame on the main (render/standard-UI) context: backend
    /// imguiNewFrame + window newImGuiFrame + DPI scale + ImGui::NewFrame, in that
    /// order. Shared by the single-threaded producer, the threaded consumer, and
    /// the threaded-session priming frame. Caller is responsible for `beginFrame`,
    /// `drainPendingFontOps`, and any ImGui mutex around this call.
    void beginMainImguiFrame();

    /// If a target FPS is set, wait until `nextDeadline` (hybrid sleep + short busy-spin)
    /// and advance it by one frame period; otherwise just refresh `nextDeadline` to now.
    /// Uses steady_clock directly (monotonic, no float drift over long sessions).
    void limitFrameRate(std::chrono::steady_clock::time_point& nextDeadline);

    std::atomic<ScreenSize> mScreenSize; ///< The screen size of the canvas (atomic: sim-thread setScreenSize vs render-thread reads).
    std::string mTitle; ///< The title of the canvas window.
    glm::vec3 mClearColor; ///< The background color of the canvas.
    unsigned int mPixelSize; ///< The pixel size on the canvas.

    ActiveBackend mBackend; ///< GPU rendering backend (compile-time selected).

    DenseLandExplorerManager   mDenseLandManager;   ///< Dense land (huge dense-world) storage + per-frame explorer pool logic.
    SparseLandExplorerManager mSparseLandManager; ///< Sparse denseLand storage + per-frame explorer pool logic.

    /// RTT passes queued by renderTo() during the update callback, executed before the main render.
    std::vector<std::pair<RenderTargetId, std::vector<BellotaId>>> mPendingRttPasses;

    PresentMode mPresentMode{DEFAULT_PRESENT_MODE}; ///< Swapchain / vsync preference (construction-time).
    std::optional<float> mTargetFps; ///< Frame-rate cap for run() (nullopt = unlimited). Runtime-settable.
    bool mStats; ///< Flag to indicate whether stats should be displayed.
    bool mHeadless{false}; ///< When true, the window is hidden (no visible UI).
    bool mSessionStarted{false}; ///< True after ensureSessionStarted() has been called.
    bool mAutoTextureGC{true}; ///< When true, unreferenced textures are removed each frame.
    bool mAutoMeshGC{true};    ///< When true, unreferenced meshes are removed each frame.

    RenderSnapshot mSnapshot;  ///< Reused POD projection of the scene for Single mode (produced + consumed by produce()/consume()).
    std::uint64_t mCommitSeq{0};        ///< Monotonic commit counter; stamped onto each snapshot.
    std::uint64_t mLastRenderedSeq{0};  ///< Highest commit seq fully rendered; gates deferred frees.

    /// A resource removed from the scene at commit `retireSeq`, awaiting GPU free.
    /// Held until `mLastRenderedSeq >= retireSeq` so no in-flight snapshot can
    /// still reference it (at depth-0 this is the same frame).
    struct PendingTextureFree      { TextureId      id; std::uint64_t retireSeq; };
    struct PendingMeshFree         { MeshId         id; std::uint64_t retireSeq; };
    struct PendingRenderTargetFree { RenderTargetId id; std::uint64_t retireSeq; };
    std::vector<PendingTextureFree>      mPendingTextureFrees;
    std::vector<PendingMeshFree>         mPendingMeshFrees;
    std::vector<PendingRenderTargetFree> mPendingRenderTargetFrees;

    int mFramebufferWidth{0};   ///< Framebuffer size captured in produce(), reused by consume().
    int mFramebufferHeight{0};

    // ----- Threaded driver state (M2 Phase A) -----
    SnapshotTripleBuffer mTripleBuffer;            ///< sim→render snapshot hand-off (lock-free).
    std::atomic<bool> mThreadedRunning{false};     ///< true while the threaded session is live.
    Canvas* mThreadedCanvas{nullptr};              ///< canvas bound by beginThreadedSession; drives explorers in produce(Threaded).
    /// Render-loop frame-time monitor for the threaded path, mirroring the local
    /// PerformanceMonitor that run() uses single-threaded: the smoothed getMS() is
    /// the dt fed to the stats overlay and to RTT ImGui timing (flushPending), so
    /// both paths report the same averaged value. Emplaced in beginThreadedSession.
    std::optional<PerformanceMonitor> mThreadedPerfMonitor;
    /// Render-thread cadence marshalled to the sim thread so the stats overlay,
    /// which is drawn into the sim-UI frame (so it survives in the cloned draw
    /// data even when the app commits its own ImGui), can report render fps/ms
    /// next to the sim commit rate (C8). Lock-free, written each consume(Threaded).
    std::atomic<float> mRenderFps{0.0f};
    std::atomic<float> mRenderMs{0.0f};
    /// Sim-thread commit-rate monitor (smoothed, same recipe as the render one).
    /// Updated each produce(Threaded) off an accumulated commit clock; read only
    /// on the sim thread for the stats overlay, so no synchronization is needed.
    std::optional<PerformanceMonitor> mSimPerfMonitor;
    float mSimClockMs{0.0f};

    /// Phase B: serializes the sim thread's runtime structural mutations
    /// (spawn/despawn → mTextures/mMeshes + usage monitors + pending-free queues)
    /// against the render thread's container access (upload/resolve/free). Held
    /// only for the fast container section — never across the vsync swap. Bellota
    /// value mutation and the snapshot projection stay lock-free.
    // Recursive: produce(Threaded) holds it around updateExplorers, whose resize
    // path calls the self-locking addBellota/removeBellota/addTexture/removeTexture
    // wrappers — so the sim thread re-enters it. The render thread only ever locks
    // it once (renderSnapshotContents).
    std::recursive_mutex mThreadedAssetMutex;

    // ----- M3: interactive ImGui on the threaded path -----
    /// Dedicated ImGui context driven on the sim thread (NewFrame/widgets/Render),
    /// sharing the main font atlas. Null until the threaded session starts. The
    /// render thread keeps the original (renderer-bearing) context; thread-local
    /// GImGui (see imconfig.h) lets the two be current on the two threads at once.
    ImGuiContext* mSimUiContext{nullptr};

    /// Main-context ImGui input snapshotted on the render thread and consumed by
    /// the sim thread, so sim-thread widgets are interactive. Guarded by its mutex.
    struct ThreadedImguiInput
    {
        float displayWidth{0.0f}, displayHeight{0.0f};
        float framebufferScaleX{1.0f}, framebufferScaleY{1.0f};
        float mouseX{0.0f}, mouseY{0.0f};
        bool  mouseDown[3]{false, false, false};
        float wheelX{0.0f}, wheelY{0.0f};   // accumulated on render, consumed+reset on sim

        // Keyboard (M4). Sized generously to avoid pulling imgui.h into this header;
        // a static_assert in the .cpp verifies it covers ImGuiKey_NamedKey_COUNT.
        static constexpr int kKeyCount = 256;
        bool keyDown[kKeyCount]{};           // indexed by (ImGuiKey - ImGuiKey_NamedKey_BEGIN)
        // Analog value per key (C11). Only the gamepad stick/trigger keys carry a
        // meaningful 0..1 value (ImGui's smooth nav); 0 for everything else. Same
        // indexing as keyDown. Lets sim-UI gamepad-stick nav match single-threaded.
        float keyAnalog[kKeyCount]{};
        bool keyCtrl{false}, keyShift{false}, keyAlt{false}, keySuper{false};
        static constexpr int kMaxTextChars = 32;
        unsigned int textChars[kMaxTextChars]{}; // chars typed this frame (consumed on sim)
        int  textCharCount{0};
        bool focused{true};
    };
    ThreadedImguiInput mThreadedImguiInput;
    std::mutex mThreadedImguiInputMutex;

    // ----- M5: gamepad input on the sim thread -----
    /// Normalized gamepad state snapshotted from the render controller on the
    /// render thread (post window poll) and replayed onto the sim controller on
    /// the sim thread, so a game whose logic runs in commit()'s update sees the
    /// gamepad. POD (no GLFW): kMaxGamepads mirrors GLFW_JOYSTICK_LAST + 1, the
    /// button/axis counts mirror the GamepadButton/GamepadAxis enums (a static
    /// assert in the .cpp verifies). Guarded by its mutex.
    struct GamepadSnapshot
    {
        static constexpr int kMaxGamepads = 16;
        static constexpr int kButtonCount = 15;
        static constexpr int kAxisCount   = 6;
        struct Pad
        {
            bool  connected{false};
            bool  buttons[kButtonCount]{};
            float axes[kAxisCount]{};
        };
        Pad pads[kMaxGamepads]{};
    };
    GamepadSnapshot mThreadedGamepadState;
    std::mutex mThreadedGamepadMutex;

    // ----- Keyboard + mouse game input on the sim thread -----
    /// Held keyboard/mouse state snapshotted from the render controller on the
    /// render thread (post window poll) and replayed onto the sim controller before
    /// the sim update, so a game running in commit()'s update can poll/receive
    /// keyboard + mouse the normal way. POD; mouse position is in canvas space
    /// (already converted render-side); scroll is the per-frame accumulated delta
    /// (consumed from the render controller). Guarded by its mutex.
    struct GameInputSnapshot
    {
        static constexpr std::size_t kKeyCount = static_cast<std::size_t>(Key::SIZEOF);
        bool      keyDown[kKeyCount]{};
        bool      mouseDown[3]{false, false, false};
        float     mouseX{0.0f}, mouseY{0.0f};
        float     scrollX{0.0f}, scrollY{0.0f};
    };
    GameInputSnapshot mThreadedGameInputState;
    std::mutex mThreadedGameInputMutex;

    /// Set from the sim-UI frame each commit; read by the host's game update (and
    /// available via Canvas) so world interaction can be suppressed while an ImGui
    /// widget has focus. One frame stale by construction (game update runs before
    /// the UI frame), which is the correct, expected behavior.
    std::atomic<bool> mImguiWantsMouse{false};
    std::atomic<bool> mImguiWantsKeyboard{false};

    /// Sim UI's desired mouse cursor shape (an ImGuiMouseCursor; int keeps imgui.h
    /// out of this header), set each commit from the sim-UI frame and applied by the
    /// render thread before its main-context NewFrame. 0 == ImGuiMouseCursor_Arrow.
    std::atomic<int> mThreadedCursor{0};

    // ----- OS clipboard marshal (threaded path) -----
    // The sim-UI clipboard callbacks run on the sim thread and can't call the
    // main-thread-only window clipboard API, so they read/write these buffers; the
    // render thread (consume) refreshes mClipboardFromOs from the OS and flushes a
    // pending mClipboardToOs to the OS. sThreadedClipboardOwner lets the plain
    // function-pointer ImGui callbacks reach the live instance.
    std::mutex mClipboardMutex;
    std::string mClipboardFromOs;                 ///< last OS clipboard text (render refreshes, throttled).
    std::optional<std::string> mClipboardToOs;    ///< pending sim→OS write, flushed render-side.
    float mLastClipboardPollTime{-1.0f};          ///< throttles the per-frame OS clipboard read.
    static FrameRunner* sThreadedClipboardOwner;  ///< target for the ImGui clipboard callbacks.
    static const char* threadedGetClipboardText(ImGuiContext* ctx);
    static void threadedSetClipboardText(ImGuiContext* ctx, const char* text);

    /// Serializes all access to the (shared) ImGui font atlas between the sim-UI
    /// context (NewFrame + widgets + Render + clone, on the sim thread) and the
    /// render/main context (NewFrame + RenderDrawData + atlas rebuild, on the
    /// render thread). ImGui 1.92's dynamic atlas is mutated every NewFrame and
    /// during glyph baking, so the two threads' ImGui sections must be mutually
    /// exclusive. Held only for the short ImGui sections — the game-sim update and
    /// the sprite render (mThreadedAssetMutex) run outside it and still overlap.
    std::mutex mImguiMutex;

    struct Window; ///< Forward declaration for window management.
    std::unique_ptr<Window> mWindow; ///< Pointer to the window object.

    AABox mLastWindowedAABox;
    ViewportRect mGameViewport; ///< Current letterboxed game viewport (set each frame in run()).

    std::optional<float> mContentScaleOverride; ///< When set, replaces the backend OS content scale.
    std::unique_ptr<ImGuiStyle> mBaseStyle;     ///< Pristine (scale-1) style sizes; reference for ScaleAllSizes.
    float mAppliedScale{1.0f};                   ///< Last scale applied to the main context's metrics.
};

}
