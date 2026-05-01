
#include "canvas_impl.h"
#include "check.h"
#include "bellota_to_mesh.h"
#include "performance_monitor.h"
#include "texture_container.h"
#include "bellota_container.h"
#include "render_target_container.h"
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
    mImguiRtt(mBackend, mRenderTargets,
              assets_Roboto_VariableFont_wdth_wght_ttf,
              assets_Roboto_VariableFont_wdth_wght_ttf_len),
    mImguiFontSize(imguiFontSize),
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

    // Font loading — must happen after ImGui platform and renderer are initialized.
    // We want main-context UI to scale with the OS DPI factor (i.e. 14 px font
    // at 100% becomes 21 px at 150%) and stay crisp. ImGui draws text quads in
    // logical (DisplaySize) coords, so a font with FontSize=N renders at
    // N*contentScale framebuffer pixels. To keep glyphs sharp, bake the atlas
    // at that final framebuffer size — that's why size_pixels is multiplied
    // by contentScale twice: once to get the logical size we want for layout,
    // once to convert that to framebuffer pixels for the atlas raster.
    // (Do NOT call style.ScaleAllSizes(contentScale) — DisplayFramebufferScale
    // already handles padding/borders; that would double-scale them.)
    ImGuiIO& io = ImGui::GetIO();
    const float contentScale = mWindow->contentScale();
    io.Fonts->AddFontFromMemoryTTF(
        assets_Roboto_VariableFont_wdth_wght_ttf,
        assets_Roboto_VariableFont_wdth_wght_ttf_len,
        imguiFontSize * contentScale * contentScale
    );

    // Bake the unscaled imguiFontSize as the default for ImGui-into-RTT contexts —
    // RTT pixels are game-canvas pixels, OS DPI is irrelevant there, so we want
    // glyphs rasterized at their logical size, not the framebuffer size. The TTF
    // buffer is bound to the cache via the manager constructor (RAII).
    mImguiRtt.fonts().setDefaultSize(imguiFontSize);
}

Canvas::CanvasImpl::~CanvasImpl()
{
    // Needs to be defined in the cpp file to avoid incomplete type errors due to the pimpl idiom for struct Window.
    // GPU resources must be freed while the GL/Vulkan context (owned by mWindow) is still alive.
    // Member destructors run after this body, so mWindow is valid here.

    // Tear down secondary ImGui contexts for any render target that hosted one.
    // Must happen before mBackend.shutdown() — the per-context backend owns GPU
    // resources (Vulkan pipeline + descriptor pool, GL shader program) that need
    // a live device/context.
    mImguiRtt.releaseAll();

    // Free meshes
    for (auto& [bellotaIndex, bellotaPack] : mBellotas)
    {
        if (bellotaPack.dmeshOpt.has_value())
            mBackend.freeMesh(bellotaPack.dmeshOpt.value());
        bellotaPack.clear();
    }

    // Render targets must be freed before textures: freeRenderTarget also removes the
    // proxy entry from the backend's texture map (without calling glDeleteTextures on it),
    // then deletes the FBO + color attachment in one call.
    for (auto& [renderTargetIndex, renderTargetPack] : mRenderTargets)
    {
        if (renderTargetPack.dRenderTargetOpt.has_value())
        {
            const TextureId proxyTexId = renderTargetPack.renderTarget.mProxyTextureId;
            if (mTextures.contains(proxyTexId.id))
            {
                const TexturePack& proxyPack = mTextures.at(proxyTexId.id);
                if (proxyPack.dtextureOpt.has_value())
                {
                    mBackend.freeRenderTarget(
                        renderTargetPack.dRenderTargetOpt.value(),
                        proxyPack.dtextureOpt.value()
                    );
                    mTextures.at(proxyTexId.id).dtextureOpt = std::nullopt;
                }
            }
        }
        renderTargetPack.clear();
    }

    for (auto& [textureIndex, texturePack] : mTextures)
    {
        if (texturePack.dtextureOpt.has_value() && !texturePack.isProxy())
            mBackend.freeTexture(texturePack.dtextureOpt.value());
        texturePack.clear();
    }

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

const ScreenSize& Canvas::CanvasImpl::screenSize() const
{
    return mScreenSize;
}

void Canvas::CanvasImpl::setScreenSize(const ScreenSize& screenSize)
{
    mScreenSize = screenSize;
}

void Canvas::CanvasImpl::setClearColor(glm::vec3 clearColor)
{
    mClearColor = clearColor;
}

void Canvas::CanvasImpl::setAutoRemoveUnusedTextures(bool enabled)
{
    mAutoTextureGC = enabled;
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

ViewportRect Canvas::CanvasImpl::gameViewport() const { return mGameViewport; }

BellotaId Canvas::CanvasImpl::addBellota(const Bellota& bellota)
{
    BellotaId newBellotaId{mBellotas.add({bellota, std::nullopt, std::nullopt})};
    Bellota& newBellota = this->bellota(newBellotaId);
    TextureId newTextureId = newBellota.texture();
    mTextureUsageMonitor.addEntry(newBellotaId, newTextureId);
    return newBellotaId;
}

void Canvas::CanvasImpl::removeBellota(const BellotaId bellotaId)
{
    BellotaPack& bellotaPackToRemove = mBellotas.at(bellotaId.id);
    TextureId textureId = bellotaPackToRemove.bellota.texture();
    if (bellotaPackToRemove.dmeshOpt.has_value())
        mBackend.freeMesh(bellotaPackToRemove.dmeshOpt.value());
    bellotaPackToRemove.clear();
    mBellotas.remove(bellotaId.id);
    mTextureUsageMonitor.removeEntry(bellotaId, textureId);
}

TextureId Canvas::CanvasImpl::addTexture(const Texture& texture)
{
    glm::ivec2 textureSize = std::visit(GetTextureSizeVisitor(), texture);
    TexturePack texturePack{texture, std::nullopt, std::nullopt, std::nullopt, textureSize};
    texturePack.mode = textureModeOf(texture);
    TextureId newTextureId{mTextures.add(std::move(texturePack))};
    const bool textureWasAdded = mTextureUsageMonitor.addUnusedTexture(newTextureId);
    debugCheck(textureWasAdded, "Texture ID already present in usage monitor — duplicate addTexture call");
    return newTextureId;
}

void Canvas::CanvasImpl::removeTexture(const TextureId textureId)
{
    const bool textureWasRemoved = mTextureUsageMonitor.removeUnusedTexture(textureId);
    debugCheck(textureWasRemoved, "Texture is not in the unused set — still referenced by a bellota or already removed");

    TexturePack& texturePackToRemove = mTextures.at(textureId.id);
    if (texturePackToRemove.dpaletteTextureOpt.has_value())
        mBackend.freePaletteTexture(texturePackToRemove.dpaletteTextureOpt.value());
    if (texturePackToRemove.dmapTextureOpt.has_value())
        mBackend.freeTileMapTexture(texturePackToRemove.dmapTextureOpt.value());
    if (texturePackToRemove.dtextureOpt.has_value())
        mBackend.freeTexture(texturePackToRemove.dtextureOpt.value());
    texturePackToRemove.clear();

    mTextures.remove(textureId.id);
}

void Canvas::CanvasImpl::clearUnusedTextures()
{
    const std::unordered_set<TextureId> unusedTextureIdsCopy = mTextureUsageMonitor.getUnusedTextureIds();
    for (TextureId textureId : unusedTextureIdsCopy)
    {
        // Proxy entries are owned by their RenderTargetPack — skip auto-cleanup.
        if (mTextures.at(textureId.id).isProxy())
            continue;
        removeTexture(textureId);
    }
    mTextureUsageMonitor.clearUnusedTextureIds();
}

void Canvas::CanvasImpl::setTexture(const BellotaId bellotaId, const TextureId textureId)
{
    const Bellota& bellotaOriginal = bellota(bellotaId);
    Bellota bellotaWithNewTexture(
        bellotaOriginal.transform(),
        textureId,
        bellotaOriginal.depthOffset()
    );
    bellotaWithNewTexture.visible() = bellotaOriginal.visible();
    bellotaWithNewTexture.currentLayer() = bellotaOriginal.currentLayer();
    replaceBellota(bellotaId, bellotaWithNewTexture);
}

void Canvas::CanvasImpl::markTextureAsDirty(const TextureId textureId)
{
    TexturePack& texturePack = mTextures.at(textureId.id);
    debugCheck(not texturePack.isProxy(), "markTextureAsDirty called on a render target proxy texture.");

    if (texturePack.dpaletteTextureOpt.has_value())
        mBackend.freePaletteTexture(texturePack.dpaletteTextureOpt.value());
    if (texturePack.dmapTextureOpt.has_value())
        mBackend.freeTileMapTexture(texturePack.dmapTextureOpt.value());
    if (texturePack.dtextureOpt.has_value())
        mBackend.freeTexture(texturePack.dtextureOpt.value());
    texturePack.clear();
}

void Canvas::CanvasImpl::setTextureMinFilter(const TextureId textureId, TextureSampleMode mode)
{
    TexturePack& texturePack = mTextures.at(textureId.id);
    texturePack.minFilter = mode;
    // Indirect index textures require GL_NEAREST — skip filter updates for them.
    if (texturePack.mode == TextureMode::Indirect) return;
    if (texturePack.dtextureOpt.has_value())
        mBackend.setTextureMinFilter(texturePack.dtextureOpt.value(), mode);
}

void Canvas::CanvasImpl::setTextureMagFilter(const TextureId textureId, TextureSampleMode mode)
{
    TexturePack& texturePack = mTextures.at(textureId.id);
    texturePack.magFilter = mode;
    // Indirect index textures require GL_NEAREST — skip filter updates for them.
    if (texturePack.mode == TextureMode::Indirect) return;
    if (texturePack.dtextureOpt.has_value())
        mBackend.setTextureMagFilter(texturePack.dtextureOpt.value(), mode);
}

RenderTargetId Canvas::CanvasImpl::addRenderTarget(ScreenSize size)
{
    const glm::ivec2 texSize{static_cast<int>(size.width), static_cast<int>(size.height)};

    // Register a proxy TexturePack so bellotas can reference the render target like any other texture.
    TexturePack proxyPack;
    proxyPack.texture = std::nullopt;
    proxyPack.dtextureOpt = std::nullopt;
    proxyPack.mTextureSize = texSize;
    TextureId proxyTexId{mTextures.add(proxyPack)};
    mTextureUsageMonitor.addUnusedTexture(proxyTexId);

    RenderTargetPack renderTargetPack;
    renderTargetPack.renderTarget = RenderTarget{texSize, proxyTexId};
    renderTargetPack.dRenderTargetOpt = std::nullopt;
    RenderTargetId newId{mRenderTargets.add(renderTargetPack)};

    return newId;
}

void Canvas::CanvasImpl::removeRenderTarget(RenderTargetId renderTargetId)
{
    RenderTargetPack& renderTargetPack = mRenderTargets.at(renderTargetId.id);
    const TextureId proxyTexId = renderTargetPack.renderTarget.mProxyTextureId;

    // Tear down the secondary ImGui context for this RTT, if one was created.
    // Must happen before mBackend.freeRenderTarget — the per-context backend
    // owns GPU resources tied to the RTT's render pass / FBO.
    mImguiRtt.releaseContext(renderTargetId);

    if (renderTargetPack.dRenderTargetOpt.has_value())
    {
        const TexturePack& proxyPack = mTextures.at(proxyTexId.id);
        debugCheck(proxyPack.dtextureOpt.has_value(), "Render target initialized but proxy texture not initialized");
        mBackend.freeRenderTarget(
            renderTargetPack.dRenderTargetOpt.value(),
            proxyPack.dtextureOpt.value()
        );
    }
    renderTargetPack.clear();

    mTextures.remove(proxyTexId.id);

    // Remove from usage monitor if currently unused (i.e. no bellotas reference it).
    mTextureUsageMonitor.removeUnusedTexture(proxyTexId);

    mRenderTargets.remove(renderTargetId.id);
}

TextureId Canvas::CanvasImpl::renderTargetTexture(RenderTargetId renderTargetId) const
{
    return mRenderTargets.at(renderTargetId.id).renderTarget.mProxyTextureId;
}

void Canvas::CanvasImpl::renderTo(RenderTargetId renderTargetId, std::vector<BellotaId> bellotaIds)
{
    mPendingRttPasses.emplace_back(renderTargetId, std::move(bellotaIds));
}

void Canvas::CanvasImpl::renderImguiTo(RenderTargetId renderTargetId, ImguiDrawCallback imguiDrawCallback)
{
    mImguiRtt.enqueue(renderTargetId, std::move(imguiDrawCallback));
}

ImFont* Canvas::CanvasImpl::bakeImguiFont(float sizePx)
{
    // Cache hit — atlas already has this size, return immediately.
    if (ImFont* cached = mImguiRtt.fonts().find(sizePx))
        return cached;

    // Cache miss + atlas unlocked (we're between Render and the next NewFrame,
    // typically pre-first-run or post-tick) — bake synchronously.
    if (!ImGui::GetIO().Fonts->Locked)
        return &mImguiRtt.fonts().bake(sizePx);

    // Cache miss + atlas locked (we're inside an ImGui frame — between NewFrame
    // and Render). Defer the bake to the next frame's drain. Caller must
    // re-poll bakeImguiFont(sameSize) on a later frame to retrieve the pointer.
    mPendingFontOps.push_back({PendingFontOp::Kind::Bake, sizePx});
    return nullptr;
}

void Canvas::CanvasImpl::removeImguiFont(float sizePx)
{
    // Always deferred — schedules a full atlas rebuild at the start of the
    // next frame. Asserting "size was baked" happens during the drain when
    // the cache's remove() runs.
    mPendingFontOps.push_back({PendingFontOp::Kind::Remove, sizePx});
}

void Canvas::CanvasImpl::drainPendingFontOps()
{
    if (mPendingFontOps.empty()) return;

    // 1. Apply Remove ops to the cache map (no atlas touch yet). Collect any
    //    pending Bake sizes for re-bake after Clear.
    std::vector<float> pendingBakes;
    for (const auto& op : mPendingFontOps)
    {
        if (op.kind == PendingFontOp::Kind::Remove)
            mImguiRtt.fonts().remove(op.sizePx);
        else
            pendingBakes.push_back(op.sizePx);
    }
    mPendingFontOps.clear();

    // 2. Snapshot remaining baked sizes from the cache (after removes).
    std::vector<float> survivors = mImguiRtt.fonts().bakedSizes();

    // 3. Wipe the atlas — every existing ImFont* is now dangling.
    ImGui::GetIO().Fonts->Clear();

    // 4. Re-add the main HiDPI font using the same recipe as the constructor.
    const float contentScale = mWindow->contentScale();
    ImGui::GetIO().Fonts->AddFontFromMemoryTTF(
        assets_Roboto_VariableFont_wdth_wght_ttf,
        assets_Roboto_VariableFont_wdth_wght_ttf_len,
        mImguiFontSize * contentScale * contentScale
    );

    // 5. Drop dangling pointers from the cache, then re-bake survivors and
    //    any pending bakes. Each bake produces a fresh ImFont*.
    mImguiRtt.fonts().invalidate();
    for (float sz : survivors)
        mImguiRtt.fonts().bake(sz);
    for (float sz : pendingBakes)
        mImguiRtt.fonts().bake(sz);

    // 6. Restore the secondary-context default to a freshly-baked font.
    mImguiRtt.fonts().setDefaultSize(mImguiFontSize);

    // 7. Refresh io.FontDefault on every alive secondary ImGuiContext —
    //    their previous pointer was invalidated by the Clear.
    mImguiRtt.refreshSecondaryContextDefaultFont();

    // 8. Drop the GPU font texture; ImGui's _NewFrame() lazily re-uploads it
    //    from the rebuilt atlas on its very next call.
    mBackend.rebuildImguiFontTexture();
}

void Canvas::CanvasImpl::setRenderTargetClearColor(RenderTargetId renderTargetId, glm::vec4 clearColor)
{
    mRenderTargets.at(renderTargetId.id).renderTarget.mClearColor = clearColor;
}

void Canvas::CanvasImpl::setTint(const BellotaId bellotaId, const Tint& tint)
{
    debugCheck(mBellotas.contains(bellotaId.id), "There is no Bellota associated with the BellotaId provided");

    BellotaPack& bellotaPack = mBellotas.at(bellotaId.id);
    bellotaPack.tintOpt = tint;
}

void Canvas::CanvasImpl::removeTint(const BellotaId bellotaId)
{
    debugCheck(mBellotas.contains(bellotaId.id), "There is no Bellota associated with the BellotaId provided");

    BellotaPack& bellotaPack = mBellotas.at(bellotaId.id);
    bellotaPack.tintOpt = std::nullopt;
}

Bellota& Canvas::CanvasImpl::bellota(BellotaId bellotaId)
{
    return mBellotas.at(bellotaId.id).bellota;
}

const Bellota& Canvas::CanvasImpl::bellota(BellotaId bellotaId) const
{
    return mBellotas.at(bellotaId.id).bellota;
}

Texture& Canvas::CanvasImpl::texture(TextureId textureId)
{
    return mTextures.at(textureId.id).texture.value();
}

const Texture& Canvas::CanvasImpl::texture(TextureId textureId) const
{
    return mTextures.at(textureId.id).texture.value();
}

bool& Canvas::CanvasImpl::stats()
{
    return mStats;
}

const bool& Canvas::CanvasImpl::stats() const
{
    return mStats;
}

DirectTexture Canvas::CanvasImpl::takeScreenshot() const
{
    const glm::ivec2 gameSize{static_cast<int>(mScreenSize.width), static_cast<int>(mScreenSize.height)};
    ScreenshotPixels pixels = mBackend.takeScreenshot(mGameViewport, gameSize);
    TextureData textureData(pixels.width, pixels.height, 1);
    std::copy(pixels.data.begin(), pixels.data.end(), textureData.getDataSpan().begin());
    return DirectTexture(std::move(textureData));
}


void Canvas::CanvasImpl::ensureSessionStarted(Controller& controller)
{
    if (mSessionStarted)
        return;
    mWindow->beginSession(controller);
    mSortedBellotaPacks.reserve(mBellotas.size() * 2);
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

void Canvas::CanvasImpl::runOneFrame(float deltaTimeMS, std::function<void(float)> update, Controller& controller)
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
    drainPendingFontOps();

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

    const glm::mat3 worldTransformMat = computeWorldTransformMat(mScreenSize);

    if (mAutoTextureGC)
        clearUnusedTextures();

    {
        ZoneScopedN("TextureUpload");
        for (auto& [textureIndex, texturePack] : mTextures)
        {
            const bool isIndirectOrTileMap =
                texturePack.mode == TextureMode::Indirect ||
                texturePack.mode == TextureMode::TileMap;

            if (texturePack.isDirty() && !texturePack.isProxy())
            {
                texturePack.dtextureOpt = mBackend.uploadTexture(
                    texturePack.texture.value(), texturePack.minFilter, texturePack.magFilter);

                if (isIndirectOrTileMap)
                {
                    auto& indirectTexture = std::get<IndirectTexture>(texturePack.texture.value());
                    texturePack.dpaletteTextureOpt = mBackend.uploadPaletteTexture(
                        indirectTexture.generatePaletteData());

                    if (texturePack.mode == TextureMode::TileMap)
                    {
                        const auto mapData = indirectTexture.generateMapData();
                        texturePack.dmapTextureOpt = mBackend.uploadTileMapTexture(
                            std::span<const std::uint8_t>(mapData), indirectTexture.mapSize());
                        mBackend.linkTileMapTextures(
                            texturePack.dtextureOpt.value(),
                            texturePack.dmapTextureOpt.value(),
                            texturePack.dpaletteTextureOpt.value());
                    }
                    else
                    {
                        mBackend.linkIndirectTextures(
                            texturePack.dtextureOpt.value(), texturePack.dpaletteTextureOpt.value());
                    }
                    indirectTexture.clearAtlasDirty();
                    indirectTexture.clearMapDirty();
                    indirectTexture.clearPaletteDirty();
                }
            }
            else if (isIndirectOrTileMap && !texturePack.isProxy() && texturePack.dtextureOpt.has_value())
            {
                auto& indirectTexture = std::get<IndirectTexture>(texturePack.texture.value());

                if (indirectTexture.isAtlasDirty() && texturePack.dpaletteTextureOpt.has_value())
                {
                    mBackend.freeTexture(texturePack.dtextureOpt.value());
                    texturePack.dtextureOpt = mBackend.uploadTexture(
                        texturePack.texture.value(), texturePack.minFilter, texturePack.magFilter);
                    if (texturePack.mode == TextureMode::TileMap && texturePack.dmapTextureOpt.has_value())
                        mBackend.linkTileMapTextures(
                            texturePack.dtextureOpt.value(),
                            texturePack.dmapTextureOpt.value(),
                            texturePack.dpaletteTextureOpt.value());
                    else
                        mBackend.linkIndirectTextures(
                            texturePack.dtextureOpt.value(), texturePack.dpaletteTextureOpt.value());
                    indirectTexture.clearAtlasDirty();
                }
                if (texturePack.mode == TextureMode::TileMap && indirectTexture.isMapDirty()
                    && texturePack.dmapTextureOpt.has_value() && texturePack.dpaletteTextureOpt.has_value())
                {
                    mBackend.freeTileMapTexture(texturePack.dmapTextureOpt.value());
                    const auto mapData = indirectTexture.generateMapData();
                    texturePack.dmapTextureOpt = mBackend.uploadTileMapTexture(
                        std::span<const std::uint8_t>(mapData), indirectTexture.mapSize());
                    mBackend.linkTileMapTextures(
                        texturePack.dtextureOpt.value(),
                        texturePack.dmapTextureOpt.value(),
                        texturePack.dpaletteTextureOpt.value());
                    indirectTexture.clearMapDirty();
                }
                if (indirectTexture.isPaletteDirty() && texturePack.dpaletteTextureOpt.has_value())
                {
                    mBackend.updatePaletteTexture(
                        texturePack.dpaletteTextureOpt.value(),
                        indirectTexture.generatePaletteData());
                    indirectTexture.clearPaletteDirty();
                }
            }
        }
    }

    for (auto& [renderTargetIndex, renderTargetPack] : mRenderTargets)
    {
        if (renderTargetPack.isDirty())
        {
            const glm::ivec2 renderTargetSize = renderTargetPack.renderTarget.mSize;
            renderTargetPack.dRenderTargetOpt = mBackend.createRenderTarget(renderTargetSize);
            const TextureId proxyTexId = renderTargetPack.renderTarget.mProxyTextureId;
            mTextures.at(proxyTexId.id).dtextureOpt =
                mBackend.getRenderTargetTexture(renderTargetPack.dRenderTargetOpt.value());
        }
    }

    {
        ZoneScopedN("MeshUpload");
        for (auto& [bellotaIndex, bellotaPack] : mBellotas)
        {
            if (bellotaPack.isDirty())
            {
                bellotaPack.meshOpt  = generateMesh(mTextures, bellotaPack.bellota);
                bellotaPack.dmeshOpt = mBackend.uploadMesh(bellotaPack.meshOpt.value());
            }
        }
    }

    {
        ZoneScopedN("DepthSort");
        sortByDepthOffset(mBellotas, mSortedBellotaPacks);
    }

    {
        ZoneScopedN("RttPasses");
        // RTT pre-passes — render requested bellotas into their render targets
        // before drawing to the main framebuffer.
        for (auto& [renderTargetId, bellotaIds] : mPendingRttPasses)
        {
            if (not mRenderTargets.contains(renderTargetId.id))
                continue;

            RenderTargetPack& renderTargetPack = mRenderTargets.at(renderTargetId.id);
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
                if (mBellotas.contains(bellotaId.id))
                    renderTargetSortedPacks.push_back(&mBellotas.at(bellotaId.id));
            }
            std::sort(renderTargetSortedPacks.begin(), renderTargetSortedPacks.end(),
                [](const BellotaPack* lhs, const BellotaPack* rhs)
                {
                    return lhs->bellota.depthOffset() < rhs->bellota.depthOffset();
                }
            );

            for (const BellotaPack* packPtr : renderTargetSortedPacks)
            {
                if (!packPtr->bellota.visible()) continue;
                if (!packPtr->dmeshOpt.has_value()) continue;
                const TexturePack& texturePack = mTextures.at(packPtr->bellota.texture().id);
                if (!texturePack.dtextureOpt.has_value()) continue;
                SpriteDrawParams drawParams = makeSpriteDrawParams(*packPtr, renderTargetWorldTransform);
                drawParams.mode = texturePack.mode;
                if ((texturePack.mode == TextureMode::Indirect || texturePack.mode == TextureMode::TileMap)
                    && texturePack.dpaletteTextureOpt.has_value())
                    drawParams.paletteTexture = texturePack.dpaletteTextureOpt.value();
                if (texturePack.mode == TextureMode::TileMap && texturePack.dmapTextureOpt.has_value())
                    drawParams.mapTexture = texturePack.dmapTextureOpt.value();
                mBackend.drawSprite(packPtr->dmeshOpt.value(), texturePack.dtextureOpt.value(), drawParams);
            }

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
        for (const BellotaPack* packPtr : mSortedBellotaPacks)
        {
            if (!packPtr->bellota.visible()) continue;
            if (!packPtr->dmeshOpt.has_value()) continue;
            const TexturePack& texturePack = mTextures.at(packPtr->bellota.texture().id);
            if (!texturePack.dtextureOpt.has_value()) continue;
            SpriteDrawParams drawParams = makeSpriteDrawParams(*packPtr, worldTransformMat);
            drawParams.mode = texturePack.mode;
            if ((texturePack.mode == TextureMode::Indirect || texturePack.mode == TextureMode::TileMap)
                && texturePack.dpaletteTextureOpt.has_value())
                drawParams.paletteTexture = texturePack.dpaletteTextureOpt.value();
            if (texturePack.mode == TextureMode::TileMap && texturePack.dmapTextureOpt.has_value())
                drawParams.mapTexture = texturePack.dmapTextureOpt.value();
            mBackend.drawSprite(packPtr->dmeshOpt.value(), texturePack.dtextureOpt.value(), drawParams);
        }
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

void Canvas::CanvasImpl::run(std::function<void(float deltaTime)> update, Controller& controller)
{
    // Always call beginSession — it resets the window close flag and rebinds
    // input callbacks, which is required after a manifest switch (canvas.close()
    // sets shouldClose=true; without reset, isRunning() returns false immediately).
    mWindow->beginSession(controller);
    if (!mSessionStarted)
    {
        mSortedBellotaPacks.reserve(mBellotas.size() * 2);
        mSessionStarted = true;
    }

    PerformanceMonitor performanceMonitor(mWindow->getTime(), 0.5f);

    while (mWindow->isRunning())
    {
        performanceMonitor.update(mWindow->getTime());
        runOneFrame(performanceMonitor.getMS(), update, controller);
    }
}

void Canvas::CanvasImpl::tick(float deltaTimeMS, std::function<void(float)> update, Controller& controller)
{
    ensureSessionStarted(controller);
    runOneFrame(deltaTimeMS, update, controller);
}

void Canvas::CanvasImpl::tick(float deltaTimeMS, std::function<void(float)> update)
{
    Controller controller;
    tick(deltaTimeMS, update, controller);
}

void Canvas::CanvasImpl::tick(float deltaTimeMS)
{
    tick(deltaTimeMS, [](float){});
}

void Canvas::CanvasImpl::close()
{
    mWindow->requestClose();
}

void Canvas::CanvasImpl::replaceBellota(const BellotaId bellotaId, const Bellota& newBellota)
{
    BellotaPack& bellotaPack = mBellotas.at(bellotaId.id);

    TextureId textureIdToReplace = bellotaPack.bellota.texture();
    const bool oldEntryRemoved = mTextureUsageMonitor.removeEntry(bellotaId, textureIdToReplace);
    debugCheck(oldEntryRemoved, "Failed to remove old texture entry from usage monitor during bellota replacement");

    TextureId newTextureId = newBellota.texture();
    const bool newEntryAdded = mTextureUsageMonitor.addEntry(bellotaId, newTextureId);
    debugCheck(newEntryAdded, "Failed to add new texture entry to usage monitor during bellota replacement");

    bellotaPack.clearMesh();
    bellotaPack.bellota = newBellota;
}

ScreenSize getPrimaryMonitorSize()
{
    return SelectedWindowBackend::getPrimaryMonitorSize();
}

}
