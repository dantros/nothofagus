
#include "canvas_impl.h"
#include "check.h"
#include "performance_monitor.h"
#include "keyboard.h"
#include "mouse.h"
#include "controller.h"
#include "roboto_font.h"
#include "backends/render_backend_select.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_transform_2d.hpp>
#include <glm/ext.hpp>
#include <imgui.h>
#include "backends/window_backend.h"
#include <ciso646>
#include <cmath>
#include <optional>
#include <vector>
#include <format>
#include <algorithm>
#include "profiling.h"

namespace Nothofagus
{

// Window is the selected backend type. Forward declared in canvas_impl.h;
// defined here so the backend headers are only included from this translation unit.
struct Canvas::CanvasImpl::Window : public SelectedWindowBackend
{
    using SelectedWindowBackend::SelectedWindowBackend;
};

static ViewportRect computeLetterboxViewport(int framebufferWidth, int framebufferHeight, unsigned int canvasWidth, unsigned int canvasHeight)
{
    const float canvasAspectRatio      = static_cast<float>(canvasWidth)      / static_cast<float>(canvasHeight);
    const float framebufferAspectRatio = static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight);
    int viewportWidth, viewportHeight, viewportX, viewportY;
    if (framebufferAspectRatio > canvasAspectRatio)
    {   // Pillarbox: framebuffer is wider than canvas — black bands left and right
        viewportHeight = framebufferHeight;
        viewportWidth  = static_cast<int>(framebufferHeight * canvasAspectRatio);
        viewportX      = (framebufferWidth - viewportWidth) / 2;
        viewportY      = 0;
    }
    else
    {   // Letterbox: framebuffer is taller than canvas — black bands top and bottom
        viewportWidth  = framebufferWidth;
        viewportHeight = static_cast<int>(framebufferWidth / canvasAspectRatio);
        viewportX      = 0;
        viewportY      = (framebufferHeight - viewportHeight) / 2;
    }
    return { viewportX, viewportY, viewportWidth, viewportHeight };
}

Canvas::CanvasImpl::CanvasImpl(
    const ScreenSize& screenSize,
    const std::string& title,
    const glm::vec3 clearColor,
    const unsigned int pixelSize,
    const float imguiFontSize,
    bool headless)
    :
    mScreenSize(screenSize),
    mTitle(title),
    mClearColor(clearColor),
    mPixelSize(pixelSize),
    mAssets(mBackend),
    mImguiRtt(mBackend, mAssets.renderTargets(),
              assets_Roboto_VariableFont_wdth_wght_ttf,
              assets_Roboto_VariableFont_wdth_wght_ttf_len,
              imguiFontSize),
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

    // ImGui platform init (GLFW or SDL3 side).
    mWindow->initImGuiPlatform();

    // Render backend init (GPU resources, shader compilation, ImGui renderer binding).
    mBackend.initialize(mWindow->nativeHandle(), {static_cast<int>(mScreenSize.width), static_cast<int>(mScreenSize.height)});
    mBackend.initImGuiRenderer();

    // Font setup — bake the main HiDPI font (imguiFontSize * contentScale²
    // for crisp DPI-aware glyphs on the main UI) and seed the secondary-context
    // default at the unscaled imguiFontSize for diegetic RTT panels.
    mImguiRtt.fonts().initialize(mWindow->contentScale());
}

Canvas::CanvasImpl::~CanvasImpl()
{
    // Defined here (not =default) to keep the pimpl idiom for struct Window working.
    // GPU resources must be freed while the GL/Vulkan context is still alive:
    // - ImGui RTT secondary contexts own per-RTT pipeline/descriptor resources
    // - mAssets owns texture/mesh/render-target GPU handles
    // mBackend.shutdown() runs last; mBackend itself is destroyed only after this body.
    mImguiRtt.releaseAll();
    mAssets.freeAllGpuResources();
    mBackend.shutdown();
}

std::size_t Canvas::CanvasImpl::getCurrentMonitor() const
{
    return mWindow->getCurrentMonitor();
}

bool Canvas::CanvasImpl::isFullscreen() const
{
    return mWindow->isFullscreen();
}

void Canvas::CanvasImpl::setFullScreenOnMonitor(std::size_t monitorIndex)
{
    mLastWindowedAABox = mWindow->getWindowAABox();
    mWindow->setFullscreenOnMonitor(monitorIndex);
}

AABox Canvas::CanvasImpl::getWindowAABox() const
{
    return mWindow->getWindowAABox();
}

void Canvas::CanvasImpl::setWindowed()
{
    mWindow->setWindowed(mLastWindowedAABox);
}

void Canvas::CanvasImpl::setWindowTitle(const std::string& title)
{
    mTitle = title;
    mWindow->setWindowTitle(title);
}

ScreenSize Canvas::CanvasImpl::windowSize() const
{
    debugCheck(mWindow != nullptr, "Canvas window has not been initialized");
    return mWindow->getWindowSize();
}

// ---------------------------------------------------------------------------
// Asset/font remove paths with cross-cutting gates. Plain forwarders are
// inlined in canvas_impl.h directly onto AssetRegistry / TilemapManager /
// mImguiRtt.fonts(); only the methods that layer a check or need imgui.h
// stay here.
// ---------------------------------------------------------------------------

void Canvas::CanvasImpl::removeBellota(const BellotaId bellotaId)
{
    debugCheck(!mTilemapManager.isExplorerManagedBellota(bellotaId.id),
        "Bellota is owned by a TilemapExplorer pool — use canvas.removeTilemapExplorer() instead of removing slot bellotas directly.");
    mAssets.removeBellota(bellotaId);
}

void Canvas::CanvasImpl::removeTexture(const TextureId textureId)
{
    debugCheck(!mTilemapManager.isExplorerManagedTexture(textureId.id),
        "Texture is owned by a TilemapExplorer pool — use canvas.removeTilemapExplorer() instead of removing slot textures directly.");
    mAssets.removeTexture(textureId);
}

void Canvas::CanvasImpl::removeRenderTarget(RenderTargetId renderTargetId)
{
    // Tear down the secondary ImGui context for this RTT first — its per-context
    // backend owns GPU resources tied to the RTT's render pass / FBO, which the
    // registry's removeRenderTarget call is about to free.
    mImguiRtt.releaseContext(renderTargetId);
    mAssets.removeRenderTarget(renderTargetId);
}

void Canvas::CanvasImpl::renderImguiTo(RenderTargetId renderTargetId, ImguiFontId fontId, ImguiDrawCallback imguiDrawCallback)
{
    // Wrap the user's callback with auto-push/pop of fontId. Graceful fallback:
    // if the bake is still pending or the id was removed, the callback runs
    // without an explicit push (the secondary-context default stays in effect).
    mImguiRtt.enqueue(renderTargetId,
        [this, fontId, cb = std::move(imguiDrawCallback)] {
            if (isImguiFontReady(fontId))
            {
                pushImguiFont(fontId);
                cb();
                popImguiFont();
            }
            else
            {
                cb();
            }
        });
}

void Canvas::CanvasImpl::pushImguiFont(ImguiFontId id)
{
    ImFont* font = getImguiFontPtr(id);
    debugCheck(font != nullptr,
        "Canvas::pushImguiFont: id is not registered or its bake is still pending — guard with isImguiFontReady()");
    ImGui::PushFont(font);
}

void Canvas::CanvasImpl::popImguiFont()
{
    ImGui::PopFont();
}

ImguiFontId Canvas::CanvasImpl::defaultImguiFontId() const
{
    auto idOpt = mImguiRtt.fonts().defaultFontId();
    debugCheck(idOpt.has_value(),
        "Canvas::defaultImguiFontId: no default font registered (CanvasImpl ctor seeds this — should never fire)");
    return *idOpt;
}

DirectTexture Canvas::CanvasImpl::takeScreenshot() const
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

void Canvas::CanvasImpl::ensureSessionStarted(Controller& controller)
{
    if (mSessionStarted)
        return;
    mWindow->beginSession(controller);
    mSortedBellotaPacks.reserve(mAssets.bellotas().size() * 2);
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
        if (!packPtr->bellota.visible()) continue;
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

void Canvas::CanvasImpl::runOneFrame(Canvas& canvas, float deltaTimeMS, std::function<void(float)> update, Controller& controller)
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
    mImguiRtt.drainPendingFontOps(mWindow->contentScale());

    // Get current framebuffer size and compute letterboxed viewport
    auto [framebufferWidth, framebufferHeight] = mWindow->getFramebufferSize();
    mGameViewport = computeLetterboxViewport(framebufferWidth, framebufferHeight, mScreenSize.width, mScreenSize.height);

    mBackend.beginFrame(mClearColor, mGameViewport, framebufferWidth, framebufferHeight);

    // Start the Dear ImGui frame
    mBackend.imguiNewFrame();
    mWindow->newImGuiFrame();
    ImGui::NewFrame();

    {
        ZoneScopedN("UserUpdate");
        update(deltaTimeMS);
    }

    {
        ZoneScopedN("TilemapExplorers");
        mTilemapManager.updateExplorers(canvas);
    }

    const glm::mat3 worldTransformMat = computeWorldTransformMat(mScreenSize);

    if (mAutoTextureGC)
        mAssets.clearUnusedTextures();

    if (mAutoMeshGC)
        mAssets.clearUnusedMeshes();

    {
        ZoneScopedN("TextureUpload");
        for (auto& [textureIndex, texturePack] : mAssets.textures())
            texturePack.syncToGpu(mBackend);
    }

    for (auto& [renderTargetIndex, renderTargetPack] : mAssets.renderTargets())
    {
        const TextureId proxyTexId = renderTargetPack.renderTarget.mProxyTextureId;
        renderTargetPack.syncToGpu(mBackend, mAssets.textures().at(proxyTexId.id));
    }

    {
        ZoneScopedN("MeshUpload");
        for (auto& [meshIndex, meshPack] : mAssets.meshes())
            meshPack.syncToGpu(mBackend);
    }

    {
        ZoneScopedN("DepthSort");
        sortByDepthOffset(mAssets.bellotas(), mSortedBellotaPacks);
    }

    {
        ZoneScopedN("RttPasses");
        // RTT pre-passes — render requested bellotas into their render targets
        // before drawing to the main framebuffer.
        for (auto& [renderTargetId, bellotaIds] : mPendingRttPasses)
        {
            if (not mAssets.renderTargets().contains(renderTargetId.id))
                continue;

            RenderTargetPack& renderTargetPack = mAssets.renderTargets().at(renderTargetId.id);
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
                if (mAssets.bellotas().contains(bellotaId.id))
                    renderTargetSortedPacks.push_back(&mAssets.bellotas().at(bellotaId.id));
            }
            std::sort(renderTargetSortedPacks.begin(), renderTargetSortedPacks.end(),
                [](const BellotaPack* lhs, const BellotaPack* rhs)
                {
                    return lhs->bellota.depthOffset() < rhs->bellota.depthOffset();
                }
            );

            drawBellotaPacks(renderTargetSortedPacks, mAssets.textures(), mAssets.meshes(), renderTargetWorldTransform, mBackend);

            mBackend.endRttPass();
        }
        mPendingRttPasses.clear();

        // ImGui-to-RTT passes — each uses a secondary ImGuiContext owned by the
        // render target, rendered with a pipeline compiled against the RTT render
        // pass (Vulkan) or into the RTT FBO (OpenGL). Lazy context creation on
        // first use; destroyed in removeRenderTarget() and the destructor.
        mImguiRtt.flushPending(deltaTimeMS, ImGui::GetIO().Fonts);
    }

    mBackend.beginMainPass(mGameViewport);

    {
        ZoneScopedN("MainDraw");
        drawBellotaPacks(mSortedBellotaPacks, mAssets.textures(), mAssets.meshes(), worldTransformMat, mBackend);
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
        mWindow->endFrame(controller, mGameViewport, mScreenSize);
    }

    FrameMark;
}

void Canvas::CanvasImpl::run(Canvas& canvas, std::function<void(float deltaTime)> update, Controller& controller)
{
    // Always call beginSession — it resets the window close flag and rebinds
    // input callbacks, which is required after a manifest switch (canvas.close()
    // sets shouldClose=true; without reset, isRunning() returns false immediately).
    mWindow->beginSession(controller);
    if (!mSessionStarted)
    {
        mSortedBellotaPacks.reserve(mAssets.bellotas().size() * 2);
        mSessionStarted = true;
    }

    PerformanceMonitor performanceMonitor(mWindow->getTime(), 0.5f);

    while (mWindow->isRunning())
    {
        performanceMonitor.update(mWindow->getTime());
        runOneFrame(canvas, performanceMonitor.getMS(), update, controller);
    }
}

void Canvas::CanvasImpl::tick(Canvas& canvas, float deltaTimeMS, std::function<void(float)> update, Controller& controller)
{
    ensureSessionStarted(controller);
    runOneFrame(canvas, deltaTimeMS, update, controller);
}

void Canvas::CanvasImpl::tick(Canvas& canvas, float deltaTimeMS, std::function<void(float)> update)
{
    Controller controller;
    tick(canvas, deltaTimeMS, update, controller);
}

void Canvas::CanvasImpl::tick(Canvas& canvas, float deltaTimeMS)
{
    tick(canvas, deltaTimeMS, [](float){});
}

void Canvas::CanvasImpl::close()
{
    mWindow->requestClose();
}

ScreenSize getPrimaryMonitorSize()
{
    return SelectedWindowBackend::getPrimaryMonitorSize();
}

}
