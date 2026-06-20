
#include "frame_runner.h"
#include "explorer_manager_impl.h"
#include "check.h"
#include "performance_monitor.h"
#include "keyboard.h"
#include "mouse.h"
#include "controller.h"
#include "asset_registry.h"
#include "imgui_rtt_manager.h"
#include "cursor_mapping.h"
#include "backends/render_backend_select.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_transform_2d.hpp>
#include <glm/ext.hpp>
#include <imgui.h>
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
    bool headless)
    :
    mScreenSize(screenSize),
    mTitle(title),
    mClearColor(clearColor),
    mPixelSize(pixelSize),
    mStats(false),
    mHeadless(headless),
    mGameViewport{0, 0, 0, 0}
{
    // Initialize the window backend (creates window, GL/Vulkan context, loads GLAD for OpenGL)
    mWindow = std::make_unique<Window>(
        mTitle,
        static_cast<int>(mScreenSize.width  * mPixelSize),
        static_cast<int>(mScreenSize.height * mPixelSize),
        !mHeadless // visible
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
    mBackend.initialize(mWindow->nativeHandle(), {static_cast<int>(mScreenSize.width), static_cast<int>(mScreenSize.height)});
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
    const glm::ivec2 gameSize{static_cast<int>(mScreenSize.width), static_cast<int>(mScreenSize.height)};
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
                                     float deltaTimeMS, std::function<void(float)> update, Controller& controller)
{
    ZoneScopedN("runOneFrame");

    const RenderSnapshot& snapshot = buildSnapshot(canvas, assets, imguiRtt, deltaTimeMS, update, controller);
    renderSnapshot(assets, imguiRtt, snapshot, deltaTimeMS, controller);

    FrameMark;
}

const RenderSnapshot& FrameRunner::buildSnapshot(Canvas& canvas, AssetRegistry& assets, ImguiRttManager& imguiRtt,
                                                 float deltaTimeMS, std::function<void(float)> update, Controller& controller)
{
    ZoneScopedN("buildSnapshot");

    {
        ZoneScopedN("Input");
        controller.processInputs();
    }

    // Drain any deferred ImGui font ops (bake-on-miss / remove) accumulated
    // since the previous frame. Atlas is guaranteed unlocked here — between
    // the previous frame's ImGui::Render() and this frame's ImGui::NewFrame().
    // Must run BEFORE mBackend.imguiNewFrame() so ImGui_Impl*_NewFrame()'s
    // lazy font-texture re-upload picks up the rebuilt atlas.
    imguiRtt.drainPendingFontOps();

    // Get current framebuffer size and compute letterboxed viewport. Stored so
    // renderSnapshot can reuse the exact same values (one getFramebufferSize per
    // frame). gameViewport() is read by user code during update (overlay
    // positioning), so it must be set before update().
    auto [framebufferWidth, framebufferHeight] = mWindow->getFramebufferSize();
    mFramebufferWidth = framebufferWidth;
    mFramebufferHeight = framebufferHeight;
    mGameViewport = computeLetterboxViewport(framebufferWidth, framebufferHeight, mScreenSize.width, mScreenSize.height);

    // M1: GPU-frame setup + ImGui NewFrame stay inline here (the user update
    // issues ImGui calls). These migrate to the render side at the thread flip.
    mBackend.beginFrame(mClearColor, mGameViewport, framebufferWidth, framebufferHeight);

    // Start the Dear ImGui frame
    mBackend.imguiNewFrame();
    mWindow->newImGuiFrame();
    applyMainContextScale(); // DPI-scale the main (standard-UI) context before NewFrame.
    ImGui::NewFrame();

    {
        ZoneScopedN("UserUpdate");
        update(deltaTimeMS);
    }

    {
        ZoneScopedN("DenseLandExplorers");
        mDenseLandManager.updateExplorers(canvas);
    }

    {
        ZoneScopedN("SparseLandExplorers");
        mSparseLandManager.updateExplorers(canvas);
    }

    // Stamp this frame's commit number, then detect unused resources and enqueue
    // them for deferred free (the actual GPU free happens in renderSnapshot, once
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

void FrameRunner::drainPendingFrees(AssetRegistry& assets, std::uint64_t lastRenderedSeq)
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
}

void FrameRunner::renderSnapshot(AssetRegistry& assets, ImguiRttManager& imguiRtt,
                                 const RenderSnapshot& snapshot, float deltaTimeMS, Controller& controller)
{
    ZoneScopedN("renderSnapshot");

    // Deferred free: release resources retired no later than the last fully
    // rendered snapshot. At depth-0 (single thread) the snapshot we are about to
    // render IS the latest commit, so frees happen this frame, before upload —
    // identical timing to the old clearUnused* path.
    mLastRenderedSeq = snapshot.commitSeq;
    drainPendingFrees(assets, mLastRenderedSeq);

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

    const glm::mat3 worldTransformMat = computeWorldTransformMat(mScreenSize);

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
        runOneFrame(canvas, assets, imguiRtt, performanceMonitor.getMS(), update, controller);
    }
}

void FrameRunner::tick(Canvas& canvas, AssetRegistry& assets, ImguiRttManager& imguiRtt,
                              float deltaTimeMS, std::function<void(float)> update, Controller& controller)
{
    ensureSessionStarted(controller);
    runOneFrame(canvas, assets, imguiRtt, deltaTimeMS, update, controller);
}

void FrameRunner::close()
{
    mWindow->requestClose();
}

// ---------------------------------------------------------------------------
// Threaded driver (M2 Phase A)
// ---------------------------------------------------------------------------

void FrameRunner::beginThreadedSession(Controller& controller)
{
    // Bind input callbacks + reset the close flag (same as run()'s session start).
    mWindow->beginSession(controller);
    if (!mSessionStarted)
    {
        mSnapshot.draws.reserve(64);
        mSessionStarted = true;
    }
    mLastRenderTimeValid = false;
    mThreadedRunning.store(true, std::memory_order_release);
}

void FrameRunner::commitFrame(AssetRegistry& assets, float deltaTimeMS, std::function<void(float)> update)
{
    // Sim thread: pure CPU. No GPU, no ImGui, no input, no explorers, no resource
    // GC (Phase A keeps resources static at runtime; the render side owns the GPU
    // containers exclusively). Only existing bellota *values* may be mutated by
    // `update`.
    ZoneScopedN("commitFrame");

    {
        ZoneScopedN("UserUpdate");
        update(deltaTimeMS);
    }

    RenderSnapshot& snapshot = mTripleBuffer.writeSlot();
    snapshot.commitSeq = ++mCommitSeq;
    snapshot.clearColor = mClearColor;

    {
        ZoneScopedN("DepthSort");
        buildMainDraws(assets.bellotas(), snapshot.draws);
    }
    {
        ZoneScopedN("RttGather");
        buildRttPasses(assets, snapshot.rttPasses);
    }

    mTripleBuffer.publish();
}

void FrameRunner::renderFrameThreaded(AssetRegistry& assets, ImguiRttManager& imguiRtt, Controller& controller)
{
    // Main thread: owns the GL/window context and does all GPU work.
    ZoneScopedN("renderFrameThreaded");

    // Dispatch input events queued by the previous frame's poll (main thread, so
    // any action callback — e.g. Escape → close() — runs here safely).
    {
        ZoneScopedN("Input");
        controller.processInputs();
    }

    imguiRtt.drainPendingFontOps();

    // Pick up the freshest published snapshot (keeps the previous one if none new).
    mTripleBuffer.acquire();
    const RenderSnapshot& snapshot = mTripleBuffer.readSlot();

    auto [framebufferWidth, framebufferHeight] = mWindow->getFramebufferSize();
    mFramebufferWidth = framebufferWidth;
    mFramebufferHeight = framebufferHeight;
    mGameViewport = computeLetterboxViewport(framebufferWidth, framebufferHeight, mScreenSize.width, mScreenSize.height);

    mBackend.beginFrame(mClearColor, mGameViewport, framebufferWidth, framebufferHeight);

    // Empty main ImGui frame — the threaded path runs no user ImGui in Phase A,
    // but a NewFrame/Render pair yields valid (empty) draw data so the shared
    // present path works. ImGui stays entirely on this (render) thread.
    mBackend.imguiNewFrame();
    mWindow->newImGuiFrame();
    applyMainContextScale();
    ImGui::NewFrame();

    const float now = mWindow->getTime();
    const float deltaTimeMS = mLastRenderTimeValid ? (now - mLastRenderTime) * 1000.0f : 0.0f;
    mLastRenderTime = now;
    mLastRenderTimeValid = true;

    renderSnapshot(assets, imguiRtt, snapshot, deltaTimeMS, controller);

    mThreadedRunning.store(mWindow->isRunning(), std::memory_order_release);

    FrameMark;
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
