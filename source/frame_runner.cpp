
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

static SpriteDrawParams makeSpriteDrawParams(const BellotaPack& pack, const glm::mat3& worldTransform)
{
    const Bellota& bellota = pack.bellota;
    const glm::mat3 totalTransform = worldTransform * bellota.transform().toMat3();
    glm::vec3 tintColor{1.0f, 1.0f, 1.0f};
    float tintIntensity = 0.0f;
    if (pack.tintOpt.has_value())
    {
        tintColor     = pack.tintOpt.value().color;
        tintIntensity = pack.tintOpt.value().intensity;
    }
    return SpriteDrawParams{
        totalTransform,
        static_cast<int>(bellota.currentLayer()),
        tintColor,
        tintIntensity,
        bellota.opacity()
    };
}

static void sortByDepthOffset(const BellotaContainer& bellotas, std::vector<const BellotaPack*>& sortedBellotas)
{
    // Per spec, clear does not change the underlaying memory allocation (capacity)
    sortedBellotas.clear();

    for (const auto& [bellotaIndex, bellotaPack] : bellotas)
    {
        if (not bellotaPack.bellota.visible()) continue;   // hidden ones never enter the sort
        sortedBellotas.push_back(&bellotaPack);
    }

    std::sort(sortedBellotas.begin(), sortedBellotas.end(),
        [](const BellotaPack* lhs, const BellotaPack* rhs)
        {
            debugCheck(lhs != nullptr and rhs != nullptr, "invalid pointers");
            const auto lhsDepthOffset = lhs->bellota.depthOffset();
            const auto rhsDepthOffset = rhs->bellota.depthOffset();
            return lhsDepthOffset < rhsDepthOffset;
        }
    );
}

static void drawBellotaPacks(
    std::span<const BellotaPack* const> sortedBellotaPacks,
    const TextureContainer& textures,
    const MeshContainer& meshes,
    const glm::mat3& worldTransform,
    ActiveBackend& backend)
{
    for (const BellotaPack* packPtr : sortedBellotaPacks)
    {
        // Callers (sortByDepthOffset / the RTT pre-pass gather) pre-filter hidden
        // bellotas, so this list is visible-only by invariant — no visible() check here.
        debugCheck(packPtr->bellota.visible());
        if (!packPtr->bellota.meshId().has_value()) continue;
        const MeshPack& meshPack = meshes.at(packPtr->bellota.meshId().value().id);
        if (!meshPack.dmeshOpt.has_value()) continue;
        const TexturePack& texturePack = textures.at(packPtr->bellota.texture().id);
        if (!texturePack.dtextureOpt.has_value()) continue;
        SpriteDrawParams drawParams = makeSpriteDrawParams(*packPtr, worldTransform);
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

    // Get current framebuffer size and compute letterboxed viewport
    auto [framebufferWidth, framebufferHeight] = mWindow->getFramebufferSize();
    mGameViewport = computeLetterboxViewport(framebufferWidth, framebufferHeight, mScreenSize.width, mScreenSize.height);

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

    const glm::mat3 worldTransformMat = computeWorldTransformMat(mScreenSize);

    if (mAutoTextureGC)
        assets.clearUnusedTextures();

    if (mAutoMeshGC)
        assets.clearUnusedMeshes();

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

    {
        ZoneScopedN("DepthSort");
        sortByDepthOffset(assets.bellotas(), mSortedBellotaPacks);
    }

    {
        ZoneScopedN("RttPasses");
        // RTT pre-passes — render requested bellotas into their render targets
        // before drawing to the main framebuffer.
        for (auto& [renderTargetId, bellotaIds] : mPendingRttPasses)
        {
            if (not assets.renderTargets().contains(renderTargetId.id))
                continue;

            RenderTargetPack& renderTargetPack = assets.renderTargets().at(renderTargetId.id);
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

            std::vector<const BellotaPack*> renderTargetSortedPacks;
            for (const BellotaId bellotaId : bellotaIds)
            {
                if (not assets.bellotas().contains(bellotaId.id)) continue;
                const BellotaPack& pack = assets.bellotas().at(bellotaId.id);
                if (not pack.bellota.visible()) continue;   // hidden ones never enter the sort
                renderTargetSortedPacks.push_back(&pack);
            }
            std::sort(renderTargetSortedPacks.begin(), renderTargetSortedPacks.end(),
                [](const BellotaPack* lhs, const BellotaPack* rhs)
                {
                    return lhs->bellota.depthOffset() < rhs->bellota.depthOffset();
                }
            );

            drawBellotaPacks(renderTargetSortedPacks, assets.textures(), assets.meshes(), renderTargetWorldTransform, mBackend);

            mBackend.endRttPass();
        }
        mPendingRttPasses.clear();

        // ImGui-to-RTT passes — each uses a secondary ImGuiContext owned by the
        // render target, rendered with a pipeline compiled against the RTT render
        // pass (Vulkan) or into the RTT FBO (OpenGL). Lazy context creation on
        // first use; destroyed in removeRenderTarget() and the destructor.
        imguiRtt.flushPending(deltaTimeMS, ImGui::GetIO().Fonts);
    }

    mBackend.beginMainPass(mGameViewport);

    {
        ZoneScopedN("MainDraw");
        drawBellotaPacks(mSortedBellotaPacks, assets.textures(), assets.meshes(), worldTransformMat, mBackend);
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
        mBackend.endFrame(ImGui::GetDrawData(), framebufferWidth, framebufferHeight);
    }

    {
        ZoneScopedN("SwapBuffers");
        mWindow->endFrame(controller, mScreenSize);
    }

    FrameMark;
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
        mSortedBellotaPacks.reserve(assets.bellotas().size() * 2);
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
