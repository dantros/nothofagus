
#include "frame_runner.h"
#include "explorer_manager_impl.h"
#include "check.h"
#include "performance_monitor.h"
#include "keyboard.h"
#include "mouse.h"
#include "controller.h"
#include "asset_registry.h"
#include "imgui_rtt_manager.h"
#include "imgui_image_manager.h"
#include "cursor_mapping.h"
#include "backends/render_backend_select.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_transform_2d.hpp>
#include <glm/ext.hpp>
#include <imgui.h>
#include "imgui_draw_clone.h"
#include "imgui_overlay.h"
#include "backends/window_backend.h"
#include <cmath>
#include <optional>
#include <vector>
#include <span>
#include <format>
#include <algorithm>
#include "profiling.h"

namespace Nothofagus
{

// Sim-UI ImGui clipboard callbacks (B7). They run on the SIM thread and so can't
// call the main-thread-only window clipboard API; instead they read/write the
// FrameRunner's marshal buffers, which the render thread syncs with the OS clipboard.
// Plain function pointers (ImGui's callback type), so they reach the live instance
// via the static sThreadedClipboardOwner set in beginThreadedSession.
FrameRunner* FrameRunner::sThreadedClipboardOwner = nullptr;

const char* FrameRunner::threadedGetClipboardText(ImGuiContext*)
{
    // ImGui requires the returned pointer to stay valid until the next call; a
    // thread_local buffer (this only runs on the sim thread) satisfies that.
    thread_local std::string buffer;
    if (sThreadedClipboardOwner != nullptr)
    {
        std::lock_guard<std::mutex> lock(sThreadedClipboardOwner->mClipboardMutex);
        buffer = sThreadedClipboardOwner->mClipboardFromOs;
    }
    return buffer.c_str();
}

void FrameRunner::threadedSetClipboardText(ImGuiContext*, const char* text)
{
    if (sThreadedClipboardOwner == nullptr)
        return;
    std::lock_guard<std::mutex> lock(sThreadedClipboardOwner->mClipboardMutex);
    sThreadedClipboardOwner->mClipboardToOs = (text != nullptr) ? std::string(text) : std::string();
}

// Window is the selected backend type. Forward declared in frame_runner.h;
// defined here so the backend headers are only included from this translation unit.
struct FrameRunner::Window : public SelectedWindowBackend
{
    using SelectedWindowBackend::SelectedWindowBackend;
};

FrameRunner::FrameRunner(
    const ScreenSize& screenSize,
    const std::string& title,
    const glm::vec3 clearColor,
    const unsigned int pixelSize,
    bool headless,
    PresentMode presentMode)
    :
    mScreenSize(screenSize),
    mTitle(title),
    mClearColor(clearColor),
    mPixelSize(pixelSize),
    mPresentMode(presentMode),
    mStats(false),
    mHeadless(headless),
    mGameViewport{0, 0, 0, 0}
{
    // Initialize the window backend (creates window, GL/Vulkan context, loads GLAD for OpenGL).
    // The GL swap interval (derived from the present mode) is applied here, while the GL
    // context is being made current; it is a no-op in Vulkan builds.
    mWindow = std::make_unique<Window>(
        mTitle,
        static_cast<int>(screenSize.width  * mPixelSize),
        static_cast<int>(screenSize.height * mPixelSize),
        !mHeadless, // visible
        presentModeToSwapInterval(mPresentMode)
    );

    // ImGui context must be created before platform/renderer bindings.
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // Snapshot the pristine (scale-1) style. applyMainContextScale() re-derives the
    // live style's spacing/padding from this reference via ScaleAllSizes whenever the
    // effective content scale changes, so repeated scaling never compounds.
    mBaseStyle = std::make_unique<ImGuiStyle>(ImGui::GetStyle());

    // ImGui platform init (GLFW or SDL3 side).
    mWindow->initImGuiPlatform();

    // Render backend init (GPU resources, shader compilation, ImGui renderer binding).
    mBackend.initialize(mWindow->nativeHandle(), {static_cast<int>(screenSize.width), static_cast<int>(screenSize.height)}, mPresentMode);
    mBackend.initImGuiRenderer();

    // Font setup happens after construction at the Canvas level — once `mAssets`
    // and `mImguiRtt` exist, Canvas calls `mImguiRtt->fonts().initialize(...)`.
}

FrameRunner::~FrameRunner()
{
    // Defined here (not =default) to keep the pimpl idiom for struct Window
    // working. Canvas's dtor is responsible for draining `mImguiRtt` and
    // `mAssets` GPU resources BEFORE destroying FrameRunner — by the time this
    // body runs they're already torn down, leaving us to shut the backend.
    // Destroy the sim-UI context (M3) first — it shares (does not own) the main
    // context's font atlas, so it must go before the main context is destroyed.
    // It has no platform/renderer backend attached, so no backend teardown is
    // needed. The caller has already joined the sim thread, so it is not current
    // on any thread.
    if (mSimUiContext != nullptr)
    {
        ImGui::DestroyContext(mSimUiContext);
        mSimUiContext = nullptr;
    }

    // Drop the clipboard-callback back-pointer if it targets this instance, so a
    // later stray callback can't dereference a destroyed FrameRunner.
    if (sThreadedClipboardOwner == this)
        sThreadedClipboardOwner = nullptr;

    mBackend.shutdown(); // detaches the ImGui renderer backend (ImGui_Impl*_Shutdown).

    // Tear the window backend down now (instead of waiting for member destruction)
    // so its destructor shuts down the ImGui *platform* backend
    // (ImGui_ImplGlfw/SDL3_Shutdown) before we destroy the context below. ImGui
    // asserts/crashes if a context is destroyed while a platform backend is still
    // attached ("Forgot to shutdown Platform backend?"). Headless has no platform
    // backend, so this is a plain window teardown there. Order is unchanged
    // otherwise: renderer shutdown -> window/surface teardown -> context destroy.
    mWindow.reset();

    // Destroy the main ImGui context this FrameRunner created in its ctor. Without
    // this each Canvas leaks a context (and its dynamic font atlas / GPU textures);
    // in a process that builds many canvases (e.g. the test suite) that
    // accumulation perturbs ImGui's global state and makes rendering flaky. The RTT
    // secondary contexts are already destroyed by ImguiRttManager before we run.
    if (ImGui::GetCurrentContext() != nullptr)
        ImGui::DestroyContext();
}

float FrameRunner::contentScale() const
{
    debugCheck(mWindow != nullptr, "FrameRunner::contentScale called before window init");
    return effectiveContentScale(mContentScaleOverride.value_or(0.0f), mWindow->contentScale());
}

void FrameRunner::applyMainContextScale()
{
    // Standard UI (the main context) honors the OS content scale so apps built on
    // Nothofagus look native on HiDPI displays. Fonts scale via FontScaleDpi (the
    // 1.92 dynamic atlas re-rasterizes at the displayed density); widget metrics
    // (padding/spacing/rounding) scale via ScaleAllSizes. RTT secondary contexts
    // keep their own style (FontScaleDpi == 1), so this never leaks into diegetic UI.
    const float scale = contentScale();
    if (scale == mAppliedScale)
        return; // Steady state: touch nothing so the dynamic font atlas stays stable.

    // Re-derive sizes from the pristine base (preserving live colors + the
    // app-controlled FontScaleMain zoom), then scale metrics. ScaleAllSizes is
    // destructive and warns against factors < 1, so clamp the metrics factor.
    ImGuiStyle& style = ImGui::GetStyle();
    ImGuiStyle scaled = *mBaseStyle;
    std::copy(std::begin(style.Colors), std::end(style.Colors), std::begin(scaled.Colors));
    scaled.FontScaleMain = style.FontScaleMain;
    const float metricsScale = (scale > 1.0f) ? scale : 1.0f;
    scaled.ScaleAllSizes(metricsScale);
    style = scaled;
    style.FontScaleDpi = scale;
    mAppliedScale = scale;
}

void FrameRunner::beginMainImguiFrame()
{
    mBackend.imguiNewFrame();
    mWindow->newImGuiFrame();
    applyMainContextScale(); // DPI-scale the main (standard-UI) context before NewFrame.
    ImGui::NewFrame();
}

std::size_t FrameRunner::getCurrentMonitor() const
{
    return mWindow->getCurrentMonitor();
}

bool FrameRunner::isFullscreen() const
{
    return mWindow->isFullscreen();
}

void FrameRunner::setFullScreenOnMonitor(std::size_t monitorIndex)
{
    mLastWindowedAABox = mWindow->getWindowAABox();
    mWindow->setFullscreenOnMonitor(monitorIndex);
}

AABox FrameRunner::getWindowAABox() const
{
    return mWindow->getWindowAABox();
}

void FrameRunner::setWindowed()
{
    mWindow->setWindowed(mLastWindowedAABox);
}

void FrameRunner::setWindowTitle(const std::string& title)
{
    mTitle = title;
    mWindow->setWindowTitle(title);
}

ScreenSize FrameRunner::windowSize() const
{
    debugCheck(mWindow != nullptr, "Canvas window has not been initialized");
    return mWindow->getWindowSize();
}

DirectTexture FrameRunner::takeScreenshot() const
{
    const ScreenSize screen = mScreenSize.load(std::memory_order_acquire);
    const glm::ivec2 gameSize{static_cast<int>(screen.width), static_cast<int>(screen.height)};
    ScreenshotPixels pixels = mBackend.takeScreenshot(mGameViewport, gameSize);
    TextureData textureData(pixels.width, pixels.height, 1);
    std::copy(pixels.data.begin(), pixels.data.end(), textureData.getDataSpan().begin());
    return DirectTexture(std::move(textureData));
}

// ---------------------------------------------------------------------------
// Frame loop
// ---------------------------------------------------------------------------

void FrameRunner::ensureSessionStarted(Controller& controller)
{
    if (mSessionStarted)
        return;
    mWindow->beginSession(controller);
    mSessionStarted = true;
}

static glm::mat3 computeWorldTransformMat(const ScreenSize& screenSize)
{
    glm::mat3 worldTransformMat(1.0);
    worldTransformMat = glm::translate(worldTransformMat, glm::vec2(-1.0, -1.0));
    const glm::vec2 worldScale(
        2.0f / screenSize.width,
        2.0f / screenSize.height
    );
    return glm::scale(worldTransformMat, worldScale);
}

static DrawItem makeDrawItem(const BellotaPack& pack)
{
    const Bellota& bellota = pack.bellota;
    debugCheck(bellota.meshId().has_value(), "BellotaPack is missing a MeshId — invariant broken");
    glm::vec3 tintColor{1.0f, 1.0f, 1.0f};
    float tintIntensity = 0.0f;
    if (pack.tintOpt.has_value())
    {
        tintColor     = pack.tintOpt.value().color;
        tintIntensity = pack.tintOpt.value().intensity;
    }
    return DrawItem{
        bellota.transform().toMat3(),
        bellota.texture(),
        bellota.meshId().value(),
        static_cast<int>(bellota.currentLayer()),
        tintColor,
        tintIntensity,
        bellota.opacity(),
        bellota.depthOffset()
    };
}

// Project the visible bellotas into a depth-sorted POD draw list. Iterating the
// same container in the same order as before and sorting on the same key keeps
// the resulting draw order identical to the previous pointer-based path.
static void buildMainDraws(const BellotaContainer& bellotas, std::vector<DrawItem>& out)
{
    out.clear(); // keeps capacity per spec

    for (const auto& [bellotaIndex, bellotaPack] : bellotas)
    {
        if (not bellotaPack.bellota.visible()) continue; // hidden ones never enter the sort
        out.push_back(makeDrawItem(bellotaPack));
    }

    std::sort(out.begin(), out.end(),
        [](const DrawItem& lhs, const DrawItem& rhs)
        {
            return lhs.depthOffset < rhs.depthOffset;
        }
    );
}

// Render a POD draw list. Resolves each item's texture/mesh id to its GPU handle
// at draw time — the snapshot only carries ids because handles do not exist yet
// at commit time.
static void drawItems(
    std::span<const DrawItem> items,
    const TextureContainer& textures,
    const MeshContainer& meshes,
    const glm::mat3& worldTransform,
    ActiveBackend& backend)
{
    for (const DrawItem& item : items)
    {
        const MeshPack& meshPack = meshes.at(item.mesh.id);
        if (!meshPack.dmeshOpt.has_value()) continue;
        const TexturePack& texturePack = textures.at(item.texture.id);
        if (!texturePack.dtextureOpt.has_value()) continue;
        SpriteDrawParams drawParams{
            worldTransform * item.bellotaTransform,
            item.layer,
            item.tintColor,
            item.tintIntensity,
            item.opacity
        };
        drawParams.mode = texturePack.mode;
        if ((texturePack.mode == TextureMode::Indirect || texturePack.mode == TextureMode::TileMap)
            && texturePack.dpaletteTextureOpt.has_value())
            drawParams.paletteTexture = texturePack.dpaletteTextureOpt.value();
        if (texturePack.mode == TextureMode::TileMap && texturePack.dmapTextureOpt.has_value())
            drawParams.mapTexture = texturePack.dmapTextureOpt.value();
        backend.drawSprite(meshPack.dmeshOpt.value(), texturePack.dtextureOpt.value(), drawParams);
    }
}

void FrameRunner::runOneFrame(Canvas& canvas, AssetRegistry& assets, ImguiRttManager& imguiRtt,
                                     ImguiImageManager& imguiImages,
                                     float deltaTimeMS, std::function<void(float)> update, Controller& controller)
{
    ZoneScopedN("runOneFrame");

    // Single-threaded frame: producer and consumer back-to-back on this thread.
    const RenderSnapshot& snapshot = produce(FrameMode::Single, &canvas, assets, &imguiRtt, &imguiImages,
                                             deltaTimeMS, std::move(update), {}, &controller);
    consume(FrameMode::Single, assets, imguiRtt, &imguiImages, snapshot, deltaTimeMS, controller);

    FrameMark;
}

const RenderSnapshot& FrameRunner::produce(FrameMode mode, Canvas* canvas, AssetRegistry& assets,
                                           ImguiRttManager* imguiRtt, ImguiImageManager* imguiImages,
                                           float deltaTimeMS,
                                           std::function<void(float)> update,
                                           std::function<void(float)> uiCallback,
                                           Controller* controller)
{
    if (mode == FrameMode::Threaded)
    {
        // Sim thread: CPU only, no GL.
        ZoneScopedN("commitFrame");

        // Stamp the commit seq BEFORE `update` so spawn/despawn can tag retired
        // resources with the commit at which they leave the scene.
        const std::uint64_t commitSeq = ++mCommitSeq;

        // Feed the sim controller from the latest input snapshots (harvested
        // render-side after the window poll) so the game `update` sees gamepad,
        // keyboard, and mouse.
        if (controller != nullptr)
        {
            ZoneScopedN("FeedInput");
            feedGamepadInput(*controller);
            feedGameInput(*controller);
        }

        // 1) Game logic — lock-free, so it overlaps the render thread's sprite work.
        //    (spawn/despawn take the asset mutex internally; bellota value writes are
        //    sim-exclusive.)
        {
            ZoneScopedN("UserUpdate");
            update(deltaTimeMS);
        }

        // 1b) Explorer pre-pass (Dense/Sparse land). It rewrites pool textures'
        //     cell maps (setMapBulk → mMapDirty) and repositions pool bellotas, all
        //     from the camera the user set in `update`. The map writes race the
        //     render thread's syncToGpu upload, so hold the asset mutex around it;
        //     the resize path re-enters via the self-locking add/remove wrappers
        //     (mThreadedAssetMutex is recursive). canvas is null only if a malformed
        //     session skipped beginThreadedSession.
        if (canvas != nullptr)
        {
            ZoneScopedN("Explorers");
            std::lock_guard<std::recursive_mutex> assetLock(mThreadedAssetMutex);
            mDenseLandManager.updateExplorers(*canvas);
            mSparseLandManager.updateExplorers(*canvas);
        }

        RenderSnapshot& snapshot = mTripleBuffer.writeSlot();
        snapshot.commitSeq = commitSeq;
        snapshot.clearColor = mClearColor;

        // 2) ImGui frame on the sim-UI context — under the ImGui mutex so it never
        //    touches the shared font atlas concurrently with the render thread.
        {
            ZoneScopedN("SimImgui");
            std::lock_guard<std::mutex> imguiLock(mImguiMutex);

            ImGui::SetCurrentContext(mSimUiContext); // thread-local current context (sim thread)
            ImGuiIO& io = ImGui::GetIO();
            io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures; // must match the atlas owner

            ThreadedImguiInput input;
            {
                std::lock_guard<std::mutex> inputLock(mThreadedImguiInputMutex);
                input = mThreadedImguiInput;
                mThreadedImguiInput.wheelX = 0.0f; // consume accumulated wheel
                mThreadedImguiInput.wheelY = 0.0f;
                mThreadedImguiInput.textCharCount = 0; // consume typed characters
            }
            const ScreenSize screen   = mScreenSize.load(std::memory_order_acquire);
            const float displayWidth  = input.displayWidth  > 0.0f ? input.displayWidth  : static_cast<float>(screen.width);
            const float displayHeight = input.displayHeight > 0.0f ? input.displayHeight : static_cast<float>(screen.height);
            io.DisplaySize = ImVec2(displayWidth, displayHeight);
            io.DisplayFramebufferScale = ImVec2(input.framebufferScaleX, input.framebufferScaleY);

            // Focus first: a loss releases held keys/mouse, avoiding stuck input.
            io.AddFocusEvent(input.focused);

            io.AddMousePosEvent(input.mouseX, input.mouseY);
            io.AddMouseButtonEvent(0, input.mouseDown[0]);
            io.AddMouseButtonEvent(1, input.mouseDown[1]);
            io.AddMouseButtonEvent(2, input.mouseDown[2]);
            if (input.wheelX != 0.0f || input.wheelY != 0.0f)
                io.AddMouseWheelEvent(input.wheelX, input.wheelY);

            // Keyboard: replay key state (ImGui dedups to changes), modifiers, text.
            for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_MouseLeft; ++key)
                io.AddKeyEvent(static_cast<ImGuiKey>(key), input.keyDown[key - ImGuiKey_NamedKey_BEGIN]);
            io.AddKeyEvent(ImGuiMod_Ctrl,  input.keyCtrl);
            io.AddKeyEvent(ImGuiMod_Shift, input.keyShift);
            io.AddKeyEvent(ImGuiMod_Alt,   input.keyAlt);
            io.AddKeyEvent(ImGuiMod_Super, input.keySuper);
            for (int i = 0; i < input.textCharCount; ++i)
                io.AddInputCharacter(input.textChars[i]);

            io.DeltaTime = std::max(deltaTimeMS * 0.001f, 1e-6f);

            ImGui::NewFrame();
            if (uiCallback)
                uiCallback(deltaTimeMS); // user ImGui widgets, on the sim thread
            ImGui::Render();

            // Publish what the UI captured this frame so the host's game update (which
            // runs before this section) can ignore that input next frame.
            mImguiWantsMouse.store(io.WantCaptureMouse, std::memory_order_release);
            mImguiWantsKeyboard.store(io.WantCaptureKeyboard, std::memory_order_release);

            // Marshal the sim UI's desired cursor shape to the render thread, which
            // applies it via the platform backend. Valid here (post-Render, before
            // the next NewFrame resets it). int storage keeps imgui.h out of the header.
            mThreadedCursor.store(static_cast<int>(ImGui::GetMouseCursor()), std::memory_order_release);

            if (!snapshot.mainUi)
                snapshot.mainUi = std::make_unique<ClonedImDrawData>();
            snapshot.mainUi->cloneFrom(ImGui::GetDrawData());
        }

        // 3) Project the scene (POD) — lock-free; the write slot is producer-owned.
        {
            ZoneScopedN("DepthSort");
            buildMainDraws(assets.bellotas(), snapshot.draws);
        }
        {
            ZoneScopedN("RttGather");
            buildRttPasses(assets, snapshot.rttPasses);
        }

        mTripleBuffer.publish();
        return snapshot;
    }

    // FrameMode::Single — one thread; ImGui runs on the main context inside `update`
    // and the consumer renders the live draw data, so the GPU + main ImGui frame are
    // opened here (before `update`). canvas/imguiRtt/controller are non-null here.
    ZoneScopedN("buildSnapshot");

    {
        ZoneScopedN("Input");
        controller->processInputs();
    }

    // Advance the ImGui-image clock before the user update issues ImGui-image draws.
    // (Single arm — imguiImages is non-null here.)
    imguiImages->beginFrame();

    // Drain any deferred ImGui font ops (bake-on-miss / remove) accumulated
    // since the previous frame. Atlas is guaranteed unlocked here — between
    // the previous frame's ImGui::Render() and this frame's ImGui::NewFrame().
    // Must run BEFORE mBackend.imguiNewFrame() so ImGui_Impl*_NewFrame()'s
    // lazy font-texture re-upload picks up the rebuilt atlas.
    imguiRtt->drainPendingFontOps();

    // Get current framebuffer size and compute letterboxed viewport. Stored so
    // the consumer can reuse the exact same values (one getFramebufferSize per
    // frame). gameViewport() is read by user code during update (overlay
    // positioning), so it must be set before update().
    auto [framebufferWidth, framebufferHeight] = mWindow->getFramebufferSize();
    mFramebufferWidth = framebufferWidth;
    mFramebufferHeight = framebufferHeight;
    const ScreenSize singleScreen = mScreenSize.load(std::memory_order_acquire);
    mGameViewport = computeLetterboxViewport(framebufferWidth, framebufferHeight, singleScreen.width, singleScreen.height);

    // M1: GPU-frame setup + ImGui NewFrame stay inline here (the user update
    // issues ImGui calls). These migrate to the render side at the thread flip.
    mBackend.beginFrame(mClearColor, mGameViewport, framebufferWidth, framebufferHeight);

    // Start the Dear ImGui frame
    beginMainImguiFrame();

    {
        ZoneScopedN("UserUpdate");
        update(deltaTimeMS);
    }

    {
        ZoneScopedN("DenseLandExplorers");
        mDenseLandManager.updateExplorers(*canvas);
    }

    {
        ZoneScopedN("SparseLandExplorers");
        mSparseLandManager.updateExplorers(*canvas);
    }

    // Stamp this frame's commit number, then detect unused resources and enqueue
    // them for deferred free (the actual GPU free happens in the consumer, once
    // no in-flight snapshot references them). At depth-0 this is the same frame.
    mSnapshot.commitSeq = ++mCommitSeq;
    mSnapshot.clearColor = mClearColor;

    if (mAutoTextureGC)
        for (TextureId textureId : assets.collectUnusedTextures())
            mPendingTextureFrees.push_back({textureId, mCommitSeq});

    if (mAutoMeshGC)
        for (MeshId meshId : assets.collectUnusedMeshes())
            mPendingMeshFrees.push_back({meshId, mCommitSeq});

    {
        ZoneScopedN("DepthSort");
        buildMainDraws(assets.bellotas(), mSnapshot.draws);
    }

    {
        ZoneScopedN("RttGather");
        buildRttPasses(assets, mSnapshot.rttPasses);
        // Internal RTT passes for registered ImGui images that need (re)rendering this frame.
        imguiImages->appendInternalPasses(mSnapshot.rttPasses);
    }

    return mSnapshot;
}

void FrameRunner::buildRttPasses(AssetRegistry& assets, std::vector<RttPass>& out)
{
    out.clear();
    for (auto& [renderTargetId, bellotaIds] : mPendingRttPasses)
    {
        RttPass pass;
        pass.target = renderTargetId;
        for (const BellotaId bellotaId : bellotaIds)
        {
            if (not assets.bellotas().contains(bellotaId.id)) continue;
            const BellotaPack& pack = assets.bellotas().at(bellotaId.id);
            if (not pack.bellota.visible()) continue;   // hidden ones never enter the sort
            pass.draws.push_back(makeDrawItem(pack));
        }
        std::sort(pass.draws.begin(), pass.draws.end(),
            [](const DrawItem& lhs, const DrawItem& rhs)
            {
                return lhs.depthOffset < rhs.depthOffset;
            }
        );
        out.push_back(std::move(pass));
    }
    mPendingRttPasses.clear();
}

void FrameRunner::drainPendingFrees(AssetRegistry& assets, ImguiRttManager& imguiRtt, std::uint64_t lastRenderedSeq)
{
    std::erase_if(mPendingTextureFrees, [&](const PendingTextureFree& pending)
    {
        if (pending.retireSeq <= lastRenderedSeq)
        {
            assets.freeRetiredTexture(pending.id);
            return true;
        }
        return false;
    });
    std::erase_if(mPendingMeshFrees, [&](const PendingMeshFree& pending)
    {
        if (pending.retireSeq <= lastRenderedSeq)
        {
            assets.freeRetiredMesh(pending.id);
            return true;
        }
        return false;
    });
    std::erase_if(mPendingRenderTargetFrees, [&](const PendingRenderTargetFree& pending)
    {
        if (pending.retireSeq <= lastRenderedSeq)
        {
            // Tear down the per-RTT ImGui secondary context (GPU + render-thread
            // affine) first, then free the FBO/VkImage + proxy texture and erase
            // both containers. Safe here on the render thread under the asset mutex.
            imguiRtt.releaseContext(pending.id);
            assets.removeRenderTarget(pending.id);
            return true;
        }
        return false;
    });
}

void FrameRunner::renderSnapshotContents(AssetRegistry& assets, ImguiRttManager& imguiRtt,
                                         ImguiImageManager* imguiImages,
                                         const RenderSnapshot& snapshot, float deltaTimeMS)
{
    // Everything here touches the asset containers (`mTextures`/`mMeshes`,
    // render targets) and the deferred-free queues. On the threaded path the
    // caller holds `mThreadedAssetMutex` around this whole method so it cannot
    // race the sim thread's structural spawns/despawns; on the single-threaded
    // path there is no contention. The vsync swap is deliberately NOT here — it
    // is done by the caller after releasing the lock.

    // Deferred free: release resources retired no later than the snapshot we are
    // about to render. Single-threaded, that snapshot is the latest commit so
    // frees happen this frame; threaded, the gate genuinely defers (render lags).
    mLastRenderedSeq = snapshot.commitSeq;
    drainPendingFrees(assets, imguiRtt, mLastRenderedSeq);

    {
        ZoneScopedN("TextureUpload");
        for (auto& [textureIndex, texturePack] : assets.textures())
            texturePack.syncToGpu(mBackend);
    }

    for (auto& [renderTargetIndex, renderTargetPack] : assets.renderTargets())
    {
        const TextureId proxyTexId = renderTargetPack.renderTarget.mProxyTextureId;
        renderTargetPack.syncToGpu(mBackend, assets.textures().at(proxyTexId.id));
    }

    {
        ZoneScopedN("MeshUpload");
        for (auto& [meshIndex, meshPack] : assets.meshes())
            meshPack.syncToGpu(mBackend);
    }

    const glm::mat3 worldTransformMat = computeWorldTransformMat(mScreenSize.load(std::memory_order_acquire));

    {
        ZoneScopedN("RttPasses");
        // RTT pre-passes — render the snapshot's RTT draw lists into their render
        // targets before drawing to the main framebuffer.
        for (const RttPass& pass : snapshot.rttPasses)
        {
            if (not assets.renderTargets().contains(pass.target.id))
                continue;

            RenderTargetPack& renderTargetPack = assets.renderTargets().at(pass.target.id);
            if (not renderTargetPack.dRenderTargetOpt.has_value())
                continue;

            const DRenderTarget& dRenderTarget = renderTargetPack.dRenderTargetOpt.value();
            const glm::vec4& clearColor = renderTargetPack.renderTarget.mClearColor;

            mBackend.beginRttPass(dRenderTarget, clearColor);

            const glm::ivec2& renderTargetSize = dRenderTarget.size;
            glm::mat3 renderTargetWorldTransform(1.0f);
            renderTargetWorldTransform = glm::translate(renderTargetWorldTransform, glm::vec2(-1.0f, -1.0f));
            renderTargetWorldTransform = glm::scale(renderTargetWorldTransform,
                glm::vec2(2.0f / renderTargetSize.x, 2.0f / renderTargetSize.y));

            drawItems(pass.draws, assets.textures(), assets.meshes(), renderTargetWorldTransform, mBackend);

            mBackend.endRttPass();
        }

        // The internal RTTs for registered ImGui images were just drawn (they are ordinary
        // RTT passes); refresh their flat-2D and create the ImGui handles the main UI samples.
        // Runs before the main ImGui render below. Null on the threaded path (ImGui images
        // are single-threaded only for now).
        if (imguiImages)
            imguiImages->resolveImages();

        // ImGui-to-RTT passes — each uses a secondary ImGuiContext owned by the
        // render target, rendered with a pipeline compiled against the RTT render
        // pass (Vulkan) or into the RTT FBO (OpenGL). Lazy context creation on
        // first use; destroyed in removeRenderTarget() and the destructor.
        imguiRtt.flushPending(deltaTimeMS, ImGui::GetIO().Fonts);
    }

    mBackend.beginMainPass(mGameViewport);

    {
        ZoneScopedN("MainDraw");
        drawItems(snapshot.draws, assets.textures(), assets.meshes(), worldTransformMat, mBackend);
    }
}

void FrameRunner::consume(FrameMode mode, AssetRegistry& assets, ImguiRttManager& imguiRtt,
                          ImguiImageManager* imguiImages,
                          const RenderSnapshot& snapshot, float deltaTimeMS, Controller& controller)
{
    if (mode == FrameMode::Threaded)
    {
        // Main thread: owns the GL/window context and does all GPU work. The caller
        // has already acquired the freshest published snapshot into `snapshot`.

        // Dispatch input events queued by the previous frame's poll (main thread, so
        // any action callback — e.g. Escape → close() — runs here safely).
        {
            ZoneScopedN("Input");
            controller.processInputs();
        }

        // OS clipboard marshal (B7): flush any sim→OS write, then refresh the OS→sim
        // cache. The OS read is throttled (it's a synchronous X11/Wayland round-trip
        // on GLFW) — our own writes are reflected into the cache immediately so
        // app-internal copy/paste is instant; cross-app paste lands within ~0.25 s.
        {
            ZoneScopedN("Clipboard");
            std::lock_guard<std::mutex> lock(mClipboardMutex);
            if (mClipboardToOs.has_value())
            {
                mWindow->setClipboardText(*mClipboardToOs);
                mClipboardFromOs = *mClipboardToOs;
                mClipboardToOs.reset();
            }
            else
            {
                const float now = mWindow->getTime();
                if (mLastClipboardPollTime < 0.0f || (now - mLastClipboardPollTime) >= 0.25f)
                {
                    mClipboardFromOs = mWindow->getClipboardText();
                    mLastClipboardPollTime = now;
                }
            }
        }

        auto [framebufferWidth, framebufferHeight] = mWindow->getFramebufferSize();
        mFramebufferWidth = framebufferWidth;
        mFramebufferHeight = framebufferHeight;
        const ScreenSize threadedScreen = mScreenSize.load(std::memory_order_acquire);
        mGameViewport = computeLetterboxViewport(framebufferWidth, framebufferHeight, threadedScreen.width, threadedScreen.height);

        mBackend.beginFrame(mClearColor, mGameViewport, framebufferWidth, framebufferHeight);

        // Smoothed render-thread frame time via the same PerformanceMonitor recipe
        // run() uses single-threaded (averaged over its period). Computed before the
        // ImGui frame so the stats overlay can show it; also fed to RTT ImGui timing.
        mThreadedPerfMonitor->update(mWindow->getTime());
        deltaTimeMS = mThreadedPerfMonitor->getMS();

        // Main-context ImGui frame on the render thread (under the ImGui mutex, so it
        // never touches the shared font atlas concurrently with the sim-UI context).
        // It does NOT draw user UI (that arrives as a clone) — it (a) drains pending
        // font ops + lets ImGui_ImplGlfw process window input so we can harvest it for
        // the sim, and (b) provides valid empty draw data for the frames before the
        // first UI commit. The optional stats overlay is drawn here on the main
        // context; it shows whenever this main frame is what gets rendered (i.e. when
        // the sim commits no UI clone — apps with sim ImGui would need stats in the
        // clone instead).
        {
            ZoneScopedN("RenderImguiNewFrame");
            std::lock_guard<std::mutex> imguiLock(mImguiMutex);
            imguiRtt.drainPendingFontOps();
            // Apply the sim UI's marshalled cursor shape before newImGuiFrame (inside
            // beginMainImguiFrame) runs the platform backend's UpdateMouseCursor, which
            // reads the main context's GetMouseCursor() and drives glfwSetCursor/SDL.
            ImGui::SetMouseCursor(static_cast<ImGuiMouseCursor>(mThreadedCursor.load(std::memory_order_acquire)));
            beginMainImguiFrame();
            harvestImguiInput(); // io.MousePos/Down/Wheel + DisplaySize are valid post-NewFrame
            if (mStats)
            {
                ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Appearing);
                ImGui::SetNextWindowSize(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
                ImGui::Begin("stats", NULL, ImGuiWindowFlags_NoTitleBar);
                ImGui::Text("%.2f fps", deltaTimeMS > 0.0f ? 1000.0f / deltaTimeMS : 0.0f);
                ImGui::Text("%.2f ms", deltaTimeMS);
                ImGui::End();
            }
            ImGui::Render();
        }

        // Container-touching section (deferred frees, GPU upload, id→handle resolve +
        // draw submission). Guarded against the sim thread's spawn/despawn. The lock
        // is released BEFORE the vsync swap so a slow present never stalls the sim.
        {
            ZoneScopedN("AssetLockedRender");
            std::lock_guard<std::recursive_mutex> assetLock(mThreadedAssetMutex);
            renderSnapshotContents(assets, imguiRtt, imguiImages, snapshot, deltaTimeMS);
        }

        {
            ZoneScopedN("ImGuiRender");
            // Render the sim's cloned UI if present; otherwise the main empty frame.
            // Under the ImGui mutex: RenderDrawData applies font-atlas texture uploads
            // (draw_data->Textures) which touch the shared atlas.
            std::lock_guard<std::mutex> imguiLock(mImguiMutex);
            ImDrawData* uiData = (snapshot.mainUi && snapshot.mainUi->hasData())
                ? snapshot.mainUi->drawData()
                : ImGui::GetDrawData();
            uiData->Textures = &ImGui::GetPlatformIO().Textures;
            mBackend.endFrame(uiData, mFramebufferWidth, mFramebufferHeight);
        }

        {
            ZoneScopedN("SwapBuffers");
            mWindow->endFrame(controller, mScreenSize.load(std::memory_order_acquire));
        }

        // Snapshot the render controller's freshly-polled input (gamepad + keyboard
        // + mouse) for the sim thread to replay onto its own controller next commit.
        {
            ZoneScopedN("HarvestInput");
            harvestGamepadInput(controller);
            harvestGameInput(controller);
        }

        mThreadedRunning.store(mWindow->isRunning(), std::memory_order_release);
        return;
    }

    // FrameMode::Single — the producer (buildSnapshot) already opened the GPU frame
    // and the main-context ImGui frame, so the consumer just renders + presents.
    renderSnapshotContents(assets, imguiRtt, imguiImages, snapshot, deltaTimeMS);

    if (mStats)
    {
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Appearing);
        ImGui::SetNextWindowSize(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
        ImGui::Begin("stats", NULL, ImGuiWindowFlags_NoTitleBar);
        ImGui::Text("%.2f fps", deltaTimeMS > 0.0f ? 1000.0f / deltaTimeMS : 0.0f);
        ImGui::Text("%.2f ms", deltaTimeMS);
        ImGui::End();
    }

    {
        ZoneScopedN("ImGuiRender");
        ImGui::Render();
        mBackend.endFrame(ImGui::GetDrawData(), mFramebufferWidth, mFramebufferHeight);
    }

    {
        ZoneScopedN("SwapBuffers");
        mWindow->endFrame(controller, mScreenSize);
    }
}


void FrameRunner::run(Canvas& canvas, AssetRegistry& assets, ImguiRttManager& imguiRtt,
                             ImguiImageManager& imguiImages,
                             std::function<void(float deltaTime)> update, Controller& controller)
{
    // Always call beginSession — it resets the window close flag and rebinds
    // input callbacks, which is required after a manifest switch (canvas.close()
    // sets shouldClose=true; without reset, isRunning() returns false immediately).
    mWindow->beginSession(controller);
    if (!mSessionStarted)
    {
        mSnapshot.draws.reserve(assets.bellotas().size() * 2);
        mSessionStarted = true;
    }

    PerformanceMonitor performanceMonitor(mWindow->getTime(), 0.5f);

    while (mWindow->isRunning())
    {
        performanceMonitor.update(mWindow->getTime());
        runOneFrame(canvas, assets, imguiRtt, imguiImages, performanceMonitor.getMS(), update, controller);
    }
}

void FrameRunner::tick(Canvas& canvas, AssetRegistry& assets, ImguiRttManager& imguiRtt,
                              ImguiImageManager& imguiImages,
                              float deltaTimeMS, std::function<void(float)> update, Controller& controller)
{
    ensureSessionStarted(controller);
    runOneFrame(canvas, assets, imguiRtt, imguiImages, deltaTimeMS, update, controller);
}

void FrameRunner::close()
{
    mWindow->requestClose();
}

// ---------------------------------------------------------------------------
// Threaded driver (M2 Phase A: snapshot hand-off; Phase B: runtime resource
// mutation guarded by mThreadedAssetMutex)
// ---------------------------------------------------------------------------

void FrameRunner::beginThreadedSession(Canvas& canvas, Controller& controller)
{
    // Remember the canvas so produce(Threaded) can drive the explorer pre-pass.
    mThreadedCanvas = &canvas;

    // Bind input callbacks + reset the close flag (same as run()'s session start).
    mWindow->beginSession(controller);
    if (!mSessionStarted)
    {
        mSnapshot.draws.reserve(64);
        mSessionStarted = true;
    }
    // Start the render-loop frame-time monitor (same period as run()'s).
    mThreadedPerfMonitor.emplace(mWindow->getTime(), 0.5f);

    // M3: create the sim-thread UI context, sharing the main font atlas, so the
    // user's ImGui widgets can run on the sim thread. It has no platform/renderer
    // backend (the sim issues no GL); textures are flagged backend-managed and are
    // actually uploaded on the render thread when the cloned draw data is rendered.
    if (mSimUiContext == nullptr)
    {
        ImGuiContext* mainContext = ImGui::GetCurrentContext();
        ImFontAtlas* sharedAtlas = ImGui::GetIO().Fonts;

        mSimUiContext = ImGui::CreateContext(sharedAtlas);
        // CreateContext restores the previous (main) context on return, so make
        // the sim-UI context current explicitly before configuring its IO.
        ImGui::SetCurrentContext(mSimUiContext);
        const ScreenSize primingScreen = mScreenSize.load(std::memory_order_acquire);
        ImGuiIO& simIo = ImGui::GetIO();
        simIo.IniFilename             = nullptr;
        simIo.BackendPlatformName     = "nothofagus_sim_ui";
        simIo.BackendFlags           |= ImGuiBackendFlags_RendererHasTextures; // atlas uploaded render-side
        // HasGamepad + NavEnableGamepad let the sim-UI process the gamepad nav keys
        // the render thread harvests from the main context and replays here (B6).
        simIo.BackendFlags           |= ImGuiBackendFlags_HasGamepad;
        simIo.DisplaySize             = ImVec2(static_cast<float>(primingScreen.width),
                                               static_cast<float>(primingScreen.height));
        // Enable keyboard + gamepad nav and wire an in-process clipboard so InputText
        // copy/paste works on the sim thread (GLFW clipboard is main-thread-only).
        simIo.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        simIo.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        // With keyboard nav on, ImGui would set io.WantCaptureKeyboard true whenever
        // io.NavActive is true — i.e. merely because a window exists with nav focus,
        // even when nothing is being typed. That makes imguiWantsKeyboard() stuck at
        // true and useless for gating game input. Disabling nav keyboard *capture*
        // keeps nav itself working (arrows/Tab/Enter move focus) but makes
        // WantCaptureKeyboard reflect real capture only — an active widget (e.g. an
        // InputText being edited) or an open modal.
        simIo.ConfigNavCaptureKeyboard = false;
        // Route the sim-UI clipboard through the OS marshal (B7): the callbacks
        // touch this instance's buffers; the render thread syncs them with the OS.
        sThreadedClipboardOwner = this;
        ImGuiPlatformIO& simPlatformIo = ImGui::GetPlatformIO();
        simPlatformIo.Platform_GetClipboardTextFn = &FrameRunner::threadedGetClipboardText;
        simPlatformIo.Platform_SetClipboardTextFn = &FrameRunner::threadedSetClipboardText;
        ImGui::SetCurrentContext(mainContext); // restore the render thread's context

        // Enable gamepad nav on the MAIN context too (threaded path only): GLFW's
        // ImGui_ImplGlfw_UpdateGamepads is gated on this flag, so without it the main
        // context's per-frame NewFrame wouldn't populate the ImGuiKey_Gamepad* keys
        // that harvestImguiInput captures and the sim replays. (SDL3 populates them
        // regardless; the flag is harmless there.) Single-threaded run/tick never
        // calls beginThreadedSession, so its main context is unaffected.
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

        // Prime the font atlas on the render thread (uploads the texture) before
        // any sim commit references it, so the shared atlas is ready and the sim's
        // NewFrame/layout never races a first-time upload.
        auto [fbW, fbH] = mWindow->getFramebufferSize();
        const ViewportRect viewport = computeLetterboxViewport(fbW, fbH, primingScreen.width, primingScreen.height);
        mBackend.beginFrame(mClearColor, viewport, fbW, fbH);
        beginMainImguiFrame();
        ImGui::Render();
        // Open the main render pass before endFrame closes it. A real frame reaches
        // beginMainPass via renderSnapshotContents; this priming frame draws nothing
        // but must still produce a balanced begin/end pass — otherwise the Vulkan
        // backend's endFrame issues vkCmdEndRenderPass with no active pass (the GL
        // backend has no render-pass concept, so it was unaffected).
        mBackend.beginMainPass(viewport);
        mBackend.endFrame(ImGui::GetDrawData(), fbW, fbH);
    }

    mThreadedRunning.store(true, std::memory_order_release);
}

void FrameRunner::commitFrame(AssetRegistry& assets, float deltaTimeMS,
                             std::function<void(float)> update, std::function<void(float)> uiCallback)
{
    // Sim thread: CPU only, no GL. The canvas (bound in beginThreadedSession) drives
    // the explorer pre-pass; imguiRtt/controller stay null (no font drain or input
    // poll on the sim thread).
    produce(FrameMode::Threaded, mThreadedCanvas, assets, nullptr, nullptr, deltaTimeMS,
            std::move(update), std::move(uiCallback), nullptr);
}

void FrameRunner::commitFrame(AssetRegistry& assets, float deltaTimeMS,
                             std::function<void(float)> update, Controller& simController)
{
    // Sim thread (M5): feed the sim controller from the latest gamepad snapshot
    // (inside produce, before update) so the game `update` sees the gamepad. No ImGui.
    produce(FrameMode::Threaded, mThreadedCanvas, assets, nullptr, nullptr, deltaTimeMS,
            std::move(update), {}, &simController);
}

void FrameRunner::renderFrameThreaded(AssetRegistry& assets, ImguiRttManager& imguiRtt, Controller& controller)
{
    ZoneScopedN("renderFrameThreaded");

    // Pick up the freshest published snapshot (keeps the previous one if none new).
    mTripleBuffer.acquire();
    const RenderSnapshot& snapshot = mTripleBuffer.readSlot();

    // dt is recomputed from the window clock inside the Threaded arm.
    consume(FrameMode::Threaded, assets, imguiRtt, nullptr, snapshot, 0.0f, controller);

    FrameMark;
}

void FrameRunner::harvestImguiInput()
{
    static_assert(ImGuiKey_NamedKey_COUNT <= ThreadedImguiInput::kKeyCount,
                  "ThreadedImguiInput::kKeyCount too small for ImGuiKey_NamedKey_COUNT");

    ImGuiIO& io = ImGui::GetIO();
    std::lock_guard<std::mutex> lock(mThreadedImguiInputMutex);
    mThreadedImguiInput.displayWidth       = io.DisplaySize.x;
    mThreadedImguiInput.displayHeight      = io.DisplaySize.y;
    mThreadedImguiInput.framebufferScaleX  = io.DisplayFramebufferScale.x;
    mThreadedImguiInput.framebufferScaleY  = io.DisplayFramebufferScale.y;
    mThreadedImguiInput.mouseX             = io.MousePos.x;
    mThreadedImguiInput.mouseY             = io.MousePos.y;
    mThreadedImguiInput.mouseDown[0]       = io.MouseDown[0];
    mThreadedImguiInput.mouseDown[1]       = io.MouseDown[1];
    mThreadedImguiInput.mouseDown[2]       = io.MouseDown[2];
    mThreadedImguiInput.wheelX            += io.MouseWheelH; // accumulate; sim consumes + resets
    mThreadedImguiInput.wheelY            += io.MouseWheel;

    // Keyboard state (M4). Iterate the named-key range, skipping the mouse-button
    // sub-range (handled above) and the reserved-mod entries — both live at the top
    // of the range from ImGuiKey_MouseLeft onward. Gamepad keys are below that and
    // harvested harmlessly (all up without a gamepad).
    for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; ++key)
    {
        const int index = key - ImGuiKey_NamedKey_BEGIN;
        mThreadedImguiInput.keyDown[index] =
            (key < ImGuiKey_MouseLeft) ? ImGui::IsKeyDown(static_cast<ImGuiKey>(key)) : false;
    }
    mThreadedImguiInput.keyCtrl  = io.KeyCtrl;
    mThreadedImguiInput.keyShift = io.KeyShift;
    mThreadedImguiInput.keyAlt   = io.KeyAlt;
    mThreadedImguiInput.keySuper = io.KeySuper;
    mThreadedImguiInput.focused  = (io.AppFocusLost == false);

    // Append this frame's typed characters (consumed + cleared by the sim).
    for (int i = 0; i < io.InputQueueCharacters.Size; ++i)
    {
        if (mThreadedImguiInput.textCharCount >= ThreadedImguiInput::kMaxTextChars)
            break;
        mThreadedImguiInput.textChars[mThreadedImguiInput.textCharCount++] =
            static_cast<unsigned int>(io.InputQueueCharacters[i]);
    }
}

void FrameRunner::harvestGamepadInput(Controller& renderController)
{
    static_assert(static_cast<int>(GamepadButton::DpadLeft) + 1 == GamepadSnapshot::kButtonCount,
                  "GamepadSnapshot::kButtonCount out of sync with the GamepadButton enum");
    static_assert(static_cast<int>(GamepadAxis::RightTrigger) + 1 == GamepadSnapshot::kAxisCount,
                  "GamepadSnapshot::kAxisCount out of sync with the GamepadAxis enum");

    // Read the render controller's normalized state into a local POD, then publish
    // it with a single mutexed copy (keeps the critical section tiny).
    GamepadSnapshot snapshot;
    for (int id = 0; id < GamepadSnapshot::kMaxGamepads; ++id)
    {
        GamepadSnapshot::Pad& pad = snapshot.pads[id];
        pad.connected = renderController.isGamepadConnected(id);
        if (!pad.connected)
            continue;
        for (int b = 0; b < GamepadSnapshot::kButtonCount; ++b)
            pad.buttons[b] = renderController.getGamepadButton(id, static_cast<GamepadButton>(b));
        for (int a = 0; a < GamepadSnapshot::kAxisCount; ++a)
            pad.axes[a] = renderController.getGamepadAxis(id, static_cast<GamepadAxis>(a));
    }

    std::lock_guard<std::mutex> lock(mThreadedGamepadMutex);
    mThreadedGamepadState = snapshot;
}

void FrameRunner::feedGamepadInput(Controller& simController)
{
    GamepadSnapshot snapshot;
    {
        std::lock_guard<std::mutex> lock(mThreadedGamepadMutex);
        snapshot = mThreadedGamepadState;
    }

    for (int id = 0; id < GamepadSnapshot::kMaxGamepads; ++id)
    {
        const GamepadSnapshot::Pad& pad = snapshot.pads[id];
        const bool wasConnected = simController.isGamepadConnected(id);

        if (pad.connected && !wasConnected)
            simController.gamepadConnected(id);
        else if (!pad.connected && wasConnected)
            simController.gamepadDisconnected(id);

        if (!pad.connected)
            continue;

        // Buttons: reconstruct press/release edges by diffing the snapshot against
        // the sim controller's current state (activateGamepadButton sets state + queues
        // the edge for processInputs() below).
        for (int b = 0; b < GamepadSnapshot::kButtonCount; ++b)
        {
            const GamepadButton button = static_cast<GamepadButton>(b);
            const bool pressed = pad.buttons[b];
            if (pressed != simController.getGamepadButton(id, button))
                simController.activateGamepadButton(
                    {id, button, pressed ? DiscreteTrigger::Press : DiscreteTrigger::Release});
        }

        // Axes: set unconditionally — updateGamepadAxis fires the axis callback only
        // when the value actually changes, so replaying a steady value is a no-op.
        for (int a = 0; a < GamepadSnapshot::kAxisCount; ++a)
            simController.updateGamepadAxis(id, static_cast<GamepadAxis>(a), pad.axes[a]);
    }

    // Dispatch the queued button edges to the game's registered callbacks.
    simController.processInputs();
}

void FrameRunner::harvestGameInput(Controller& renderController)
{
    // Read the render controller's held keyboard/mouse state + per-frame scroll
    // into a local, then publish under the mutex. Scroll is a DELTA, so accumulate
    // it into the shared state (the sim resets it on consume) rather than overwrite.
    GameInputSnapshot local;
    for (std::size_t k = 0; k < GameInputSnapshot::kKeyCount; ++k)
        local.keyDown[k] = renderController.isKeyDown(static_cast<Key>(k));
    for (std::size_t b = 0; b < 3; ++b)
        local.mouseDown[b] = renderController.isMouseButtonDown(static_cast<MouseButton>(b));
    const glm::vec2 mousePos = renderController.getMousePosition();
    local.mouseX = mousePos.x;
    local.mouseY = mousePos.y;
    const glm::vec2 scroll = renderController.consumeScroll();

    std::lock_guard<std::mutex> lock(mThreadedGameInputMutex);
    std::copy(std::begin(local.keyDown), std::end(local.keyDown), std::begin(mThreadedGameInputState.keyDown));
    std::copy(std::begin(local.mouseDown), std::end(local.mouseDown), std::begin(mThreadedGameInputState.mouseDown));
    mThreadedGameInputState.mouseX = local.mouseX;
    mThreadedGameInputState.mouseY = local.mouseY;
    mThreadedGameInputState.scrollX += scroll.x;
    mThreadedGameInputState.scrollY += scroll.y;
}

void FrameRunner::feedGameInput(Controller& simController)
{
    GameInputSnapshot snapshot;
    {
        std::lock_guard<std::mutex> lock(mThreadedGameInputMutex);
        snapshot = mThreadedGameInputState;
        mThreadedGameInputState.scrollX = 0.0f; // consume accumulated scroll
        mThreadedGameInputState.scrollY = 0.0f;
    }

    // Keyboard: reconstruct press/release edges by diffing the snapshot against the
    // sim controller's held state (activate sets state + queues the edge).
    for (std::size_t k = 0; k < GameInputSnapshot::kKeyCount; ++k)
    {
        const Key key = static_cast<Key>(k);
        const bool down = snapshot.keyDown[k];
        if (down != simController.isKeyDown(key))
            simController.activate({key, down ? DiscreteTrigger::Press : DiscreteTrigger::Release});
    }

    // Mouse buttons: same edge reconstruction.
    for (std::size_t b = 0; b < 3; ++b)
    {
        const MouseButton button = static_cast<MouseButton>(b);
        const bool down = snapshot.mouseDown[b];
        if (down != simController.isMouseButtonDown(button))
            simController.activateMouseButton({button, down ? DiscreteTrigger::Press : DiscreteTrigger::Release});
    }

    // Mouse position (already canvas-space): only on change, to match the
    // single-threaded path (the backend updates it per move event, not per frame).
    const glm::vec2 newPos(snapshot.mouseX, snapshot.mouseY);
    if (newPos != simController.getMousePosition())
        simController.updateMousePosition(newPos);

    // Scroll: forward this frame's accumulated delta (fires the scroll callback).
    if (snapshot.scrollX != 0.0f || snapshot.scrollY != 0.0f)
        simController.scrolled(glm::vec2(snapshot.scrollX, snapshot.scrollY));

    // Dispatch the queued key / mouse-button edges to the game's callbacks.
    simController.processInputs();
}

Nothofagus::BellotaId FrameRunner::addBellota(AssetRegistry& assets, const Bellota& bellota)
{
    // Structural mutation of the asset containers (adds the bellota plus its
    // auto-quad mesh and registers usage entries). When a threaded session is live
    // this may run on the sim thread concurrently with the render thread's
    // container access, so take the asset mutex; single-threaded (run/tick or
    // setup) there is no other party and we skip the lock entirely.
    std::unique_lock<std::recursive_mutex> lock;
    if (mThreadedRunning.load(std::memory_order_acquire))
        lock = std::unique_lock<std::recursive_mutex>(mThreadedAssetMutex);
    return assets.addBellota(bellota);
}

void FrameRunner::removeBellota(AssetRegistry& assets, BellotaId bellotaId)
{
    // In a live threaded session: take the asset mutex, remove, then detect any
    // texture/mesh the bellota orphaned (typically its auto-quad mesh) and queue
    // them for deferred GPU free — freed render-side once no in-flight snapshot
    // still references them (retireSeq <= lastRenderedSeq). Tag with the current
    // commit seq: the snapshot built this commit no longer references the removed
    // bellota.
    if (mThreadedRunning.load(std::memory_order_acquire))
    {
        std::lock_guard<std::recursive_mutex> lock(mThreadedAssetMutex);
        assets.removeBellota(bellotaId);
        for (TextureId textureId : assets.collectUnusedTextures())
            mPendingTextureFrees.push_back({textureId, mCommitSeq});
        for (MeshId meshId : assets.collectUnusedMeshes())
            mPendingMeshFrees.push_back({meshId, mCommitSeq});
        return;
    }
    // Single-threaded: plain remove; orphaned resources are GC'd by the
    // produce(Single) collectUnused* pass on the next frame (unchanged behavior).
    assets.removeBellota(bellotaId);
}

std::unique_lock<std::recursive_mutex> FrameRunner::lockAssetsIfThreaded()
{
    if (mThreadedRunning.load(std::memory_order_acquire))
        return std::unique_lock<std::recursive_mutex>(mThreadedAssetMutex);
    return std::unique_lock<std::recursive_mutex>();
}

TextureId FrameRunner::addTexture(AssetRegistry& assets, const Texture& texture)
{
    auto lock = lockAssetsIfThreaded();
    return assets.addTexture(texture); // GPU upload is lazy (syncToGpu), so add is sim-safe.
}

void FrameRunner::removeTexture(AssetRegistry& assets, TextureId textureId)
{
    // Threaded: do only the usage-monitor bookkeeping now and defer the GPU free +
    // container erase to the render thread (drainPendingFrees -> freeRetiredTexture),
    // so an in-flight snapshot referencing this id is never freed mid-render.
    if (mThreadedRunning.load(std::memory_order_acquire))
    {
        std::lock_guard<std::recursive_mutex> lock(mThreadedAssetMutex);
        // Queue only if retire actually removed it from the unused set; a prior
        // removeBellota sweep may have already collected it (tolerant — no double free).
        if (assets.retireTexture(textureId))
            mPendingTextureFrees.push_back({textureId, mCommitSeq});
        return;
    }
    assets.removeTexture(textureId);
}

void FrameRunner::setTexture(AssetRegistry& assets, BellotaId bellotaId, TextureId textureId)
{
    auto lock = lockAssetsIfThreaded();
    assets.setTexture(bellotaId, textureId);
}

MeshId FrameRunner::addMesh(AssetRegistry& assets, const Mesh& mesh)
{
    auto lock = lockAssetsIfThreaded();
    return assets.addMesh(mesh);
}

MeshId FrameRunner::addMesh(AssetRegistry& assets, Mesh&& mesh)
{
    auto lock = lockAssetsIfThreaded();
    return assets.addMesh(std::move(mesh));
}

void FrameRunner::removeMesh(AssetRegistry& assets, MeshId meshId)
{
    if (mThreadedRunning.load(std::memory_order_acquire))
    {
        std::lock_guard<std::recursive_mutex> lock(mThreadedAssetMutex);
        if (assets.retireMesh(meshId))
            mPendingMeshFrees.push_back({meshId, mCommitSeq});
        return;
    }
    assets.removeMesh(meshId);
}

void FrameRunner::setMesh(AssetRegistry& assets, BellotaId bellotaId, MeshId meshId)
{
    auto lock = lockAssetsIfThreaded();
    assets.setMesh(bellotaId, meshId);
}

RenderTargetId FrameRunner::addRenderTarget(AssetRegistry& assets, ScreenSize size)
{
    auto lock = lockAssetsIfThreaded();
    return assets.addRenderTarget(size); // GPU creation is lazy (syncToGpu), so add is sim-safe.
}

void FrameRunner::removeRenderTarget(AssetRegistry& assets, RenderTargetId renderTargetId)
{
    // Threaded: defer the whole removeRenderTarget (it frees GPU + erases the RT and
    // its proxy texture immediately) to the render thread. The RT + proxy stay live
    // for any in-flight snapshot until drainPendingFrees runs it at a safe seq.
    if (mThreadedRunning.load(std::memory_order_acquire))
    {
        std::lock_guard<std::recursive_mutex> lock(mThreadedAssetMutex);
        mPendingRenderTargetFrees.push_back({renderTargetId, mCommitSeq});
        return;
    }
    assets.removeRenderTarget(renderTargetId);
}

void FrameRunner::setRenderTargetClearColor(AssetRegistry& assets, RenderTargetId renderTargetId, glm::vec4 clearColor)
{
    // The render thread reads the RT clear color live during the RTT pass (under the
    // asset mutex), so serialize the write when threaded.
    auto lock = lockAssetsIfThreaded();
    assets.setRenderTargetClearColor(renderTargetId, clearColor);
}

ScreenSize getPrimaryMonitorSize()
{
    return SelectedWindowBackend::getPrimaryMonitorSize();
}

float getPrimaryMonitorContentScale()
{
    return SelectedWindowBackend::getPrimaryMonitorContentScale();
}

// Explicit instantiations — co-located with the matching extern template
// declarations in frame_runner.h. Any third backend added later only needs a
// `static_assert(LandType<X>);` in its source + a line here and a matching
// extern decl in frame_runner.h. Spelled with the template-id (not the
// DenseLandExplorerManager / SparseLandExplorerManager aliases) because explicit
// instantiation does not accept typedef-names.
template class ExplorerManager<DenseLand>;
template class ExplorerManager<SparseLand>;

}
