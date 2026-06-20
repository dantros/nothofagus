#include "canvas.h"
#include "frame_runner.h"
#include "asset_registry.h"
#include "imgui_rtt_manager.h"
#include "check.h"
#include "embedded_fonts.h"
#include <imgui.h>

namespace Nothofagus
{

/// Build the embedded font family from the generated compressed blobs. The
/// five Latin faces are always present; each CJK face is included only when
/// its NOTHOFAGUS_HAS_CJK_* define is set (driven by the matching CMake
/// option). All blobs are stb-compressed; ImguiFontManager bakes them via
/// AddFontFromMemoryCompressedTTF.
static EmbeddedFontFamily makeEmbeddedFontFamily()
{
    EmbeddedFontFamily family{
        { notoSansRegular_compressed_data,    notoSansRegular_compressed_size },
        { notoSansBold_compressed_data,       notoSansBold_compressed_size },
        { notoSansItalic_compressed_data,     notoSansItalic_compressed_size },
        { notoSansBoldItalic_compressed_data, notoSansBoldItalic_compressed_size },
        { notoSansMono_compressed_data,       notoSansMono_compressed_size },
        {},
    };
#ifdef NOTHOFAGUS_HAS_CJK_SC
    family.cjk.push_back({ { notoSansCjkSc_compressed_data, notoSansCjkSc_compressed_size },
                           GlyphRange::ChineseSimplifiedCommon, CjkScript::SimplifiedChinese });
#endif
#ifdef NOTHOFAGUS_HAS_CJK_TC
    family.cjk.push_back({ { notoSansCjkTc_compressed_data, notoSansCjkTc_compressed_size },
                           GlyphRange::ChineseFull, CjkScript::TraditionalChinese });
#endif
#ifdef NOTHOFAGUS_HAS_CJK_JP
    family.cjk.push_back({ { notoSansCjkJp_compressed_data, notoSansCjkJp_compressed_size },
                           GlyphRange::Japanese, CjkScript::Japanese });
#endif
#ifdef NOTHOFAGUS_HAS_CJK_KR
    family.cjk.push_back({ { notoSansCjkKr_compressed_data, notoSansCjkKr_compressed_size },
                           GlyphRange::Korean, CjkScript::Korean });
#endif
    return family;
}

/**
 * @struct Canvas::Implementation
 * @brief Single nested pimpl holding every internal collaborator by value.
 *        Owning a single Implementation struct lets `canvas.h` expose only this
 *        one nested forward declaration — `Nothofagus::FrameRunner`,
 *        `Nothofagus::AssetRegistry`, and `Nothofagus::ImguiRttManager` never
 *        appear in the public header.
 *
 * Member declaration order = construction order:
 *   `frameRunner` first (builds mBackend, creates window, init ImGui)
 *   → `assets` (binds to frameRunner.backend())
 *   → `imguiRtt` (binds to frameRunner.backend() and assets.renderTargets()).
 *
 * Reverse-destruction matches the GPU teardown sequence (ImGui RTT contexts
 * → asset GPU handles → ~FrameRunner runs mBackend.shutdown()).
 */
struct Canvas::Implementation
{
    Implementation(
        const ScreenSize& screenSize,
        const std::string& title,
        const glm::vec3 clearColor,
        const unsigned int pixelSize,
        const float imguiFontSize,
        bool headless)
        : frameRunner(screenSize, title, clearColor, pixelSize, headless),
          assets(frameRunner.backend()),
          imguiRtt(frameRunner.backend(), assets.renderTargets(),
                   makeEmbeddedFontFamily(),
                   imguiFontSize)
    {
        // Main UI font bake — needs the backend's ImGui renderer to be live,
        // which it is once FrameRunner's ctor has returned. HiDPI is applied at
        // the context level each frame (FrameRunner::applyMainContextScale), so
        // fonts are baked at their logical sizes here.
        imguiRtt.fonts().initialize();
    }

    FrameRunner      frameRunner;
    AssetRegistry    assets;
    ImguiRttManager  imguiRtt;
};

Canvas::Canvas(
    const ScreenSize& screenSize,
    const std::string& title,
    const glm::vec3 clearColor,
    const unsigned int pixelSize,
    const float imguiFontSize,
    bool headless
)
    : mImplPtr(std::make_unique<Implementation>(
          screenSize, title, clearColor, pixelSize, imguiFontSize, headless))
{
}

Canvas::~Canvas()
{
    // GPU resources must be freed while the backend is still alive. Drain
    // imguiRtt and assets explicitly here; the by-value members inside
    // Implementation then destroy in reverse declaration order
    // (imguiRtt → assets → frameRunner, where the last one shuts the backend down).
    mImplPtr->imguiRtt.releaseAll();
    mImplPtr->assets.freeAllGpuResources();
}

// ---------------------------------------------------------------------------
// Window / display — forward to FrameRunner
// ---------------------------------------------------------------------------

std::size_t Canvas::getCurrentMonitor() const                   { return mImplPtr->frameRunner.getCurrentMonitor(); }
bool Canvas::isFullscreen() const                               { return mImplPtr->frameRunner.isFullscreen(); }
void Canvas::setFullScreenOnMonitor(std::size_t monitor)        { mImplPtr->frameRunner.setFullScreenOnMonitor(monitor); }
void Canvas::setWindowed()                                       { mImplPtr->frameRunner.setWindowed(); }
const ScreenSize& Canvas::screenSize() const                    { return mImplPtr->frameRunner.screenSize(); }
void Canvas::setScreenSize(const ScreenSize& screenSize)        { mImplPtr->frameRunner.setScreenSize(screenSize); }
void Canvas::setClearColor(glm::vec3 clearColor)                { mImplPtr->frameRunner.setClearColor(clearColor); }
void Canvas::setAutoRemoveUnusedTextures(bool enabled)          { mImplPtr->frameRunner.setAutoRemoveUnusedTextures(enabled); }
void Canvas::setAutoRemoveUnusedMeshes(bool enabled)            { mImplPtr->frameRunner.setAutoRemoveUnusedMeshes(enabled); }
void Canvas::setWindowTitle(const std::string& title)           { mImplPtr->frameRunner.setWindowTitle(title); }
ScreenSize Canvas::windowSize() const                            { return mImplPtr->frameRunner.windowSize(); }
ViewportRect Canvas::gameViewport() const                       { return mImplPtr->frameRunner.gameViewport(); }

ImguiOverlayRect Canvas::imguiOverlayViewport() const
{
    const ViewportRect viewport = mImplPtr->frameRunner.gameViewport();
    const ImGuiIO& io = ImGui::GetIO();
    return computeImguiOverlayViewport(
        viewport.x, viewport.y, viewport.width, viewport.height,
        io.DisplaySize.x, io.DisplaySize.y,
        io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
}

float Canvas::imguiBaseFontSize() const                          { return mImplPtr->imguiRtt.fonts().imguiFontSize(); }
float Canvas::imguiScaledFontSize() const                        { return imguiBaseFontSize() * contentScale(); }
float Canvas::contentScale() const                               { return mImplPtr->frameRunner.contentScale(); }
void Canvas::setContentScaleOverride(std::optional<float> scale) { mImplPtr->frameRunner.setContentScaleOverride(scale); }

// ---------------------------------------------------------------------------
// Bellotas — forward to AssetRegistry; remove gates against DenseLandExplorer pool
// ---------------------------------------------------------------------------

BellotaId Canvas::addBellota(const Bellota& bellota)            { return mImplPtr->assets.addBellota(bellota); }

void Canvas::removeBellota(const BellotaId bellotaId)
{
    debugCheck(!mImplPtr->frameRunner.isExplorerManagedBellota(bellotaId.id),
        "Bellota is owned by an explorer pool — use canvas.removeDenseLandExplorer() / canvas.removeSparseLandExplorer() instead of removing slot bellotas directly.");
    mImplPtr->assets.removeBellota(bellotaId);
}

Bellota& Canvas::bellota(BellotaId bellotaId)                   { return mImplPtr->assets.bellota(bellotaId); }
const Bellota& Canvas::bellota(BellotaId bellotaId) const       { return mImplPtr->assets.bellota(bellotaId); }
void Canvas::setTint(const BellotaId bellotaId, const Tint& tint) { mImplPtr->assets.setTint(bellotaId, tint); }
void Canvas::removeTint(const BellotaId bellotaId)              { mImplPtr->assets.removeTint(bellotaId); }

// ---------------------------------------------------------------------------
// Textures — forward to AssetRegistry; remove gates against DenseLandExplorer pool
// ---------------------------------------------------------------------------

TextureId Canvas::addTexture(const Texture& texture)            { return mImplPtr->assets.addTexture(texture); }

void Canvas::removeTexture(const TextureId textureId)
{
    debugCheck(!mImplPtr->frameRunner.isExplorerManagedTexture(textureId.id),
        "Texture is owned by an explorer pool — use canvas.removeDenseLandExplorer() / canvas.removeSparseLandExplorer() instead of removing slot textures directly.");
    mImplPtr->assets.removeTexture(textureId);
}

void Canvas::setTexture(const BellotaId bellotaId, const TextureId textureId)            { mImplPtr->assets.setTexture(bellotaId, textureId); }
void Canvas::markTextureAsDirty(const TextureId textureId)                                { mImplPtr->assets.markTextureAsDirty(textureId); }
void Canvas::setTextureMinFilter(const TextureId textureId, TextureSampleMode mode)       { mImplPtr->assets.setTextureMinFilter(textureId, mode); }
void Canvas::setTextureMagFilter(const TextureId textureId, TextureSampleMode mode)       { mImplPtr->assets.setTextureMagFilter(textureId, mode); }
Texture& Canvas::texture(TextureId textureId)                                             { return mImplPtr->assets.texture(textureId); }
const Texture& Canvas::texture(TextureId textureId) const                                 { return mImplPtr->assets.texture(textureId); }

// ---------------------------------------------------------------------------
// Meshes — forward to AssetRegistry
// ---------------------------------------------------------------------------

MeshId Canvas::addMesh(const Mesh& mesh)                                                  { return mImplPtr->assets.addMesh(mesh); }
MeshId Canvas::addMesh(Mesh&& mesh)                                                       { return mImplPtr->assets.addMesh(std::move(mesh)); }
void Canvas::removeMesh(MeshId meshId)                                                    { mImplPtr->assets.removeMesh(meshId); }
void Canvas::setMesh(const BellotaId bellotaId, const MeshId meshId)                      { mImplPtr->assets.setMesh(bellotaId, meshId); }
const Mesh& Canvas::mesh(MeshId meshId) const                                             { return mImplPtr->assets.mesh(meshId); }
const Mesh& Canvas::mesh(BellotaId bellotaId) const                                       { return mImplPtr->assets.mesh(bellotaId); }

// ---------------------------------------------------------------------------
// Render targets — forward to AssetRegistry; remove tears down the per-RTT
// ImGui secondary context first (its backend resources are tied to the RTT
// render pass / FBO that the registry is about to free).
// ---------------------------------------------------------------------------

RenderTargetId Canvas::addRenderTarget(ScreenSize size)                                   { return mImplPtr->assets.addRenderTarget(size); }

void Canvas::removeRenderTarget(RenderTargetId renderTargetId)
{
    mImplPtr->imguiRtt.releaseContext(renderTargetId);
    mImplPtr->assets.removeRenderTarget(renderTargetId);
}

TextureId Canvas::renderTargetTexture(RenderTargetId renderTargetId) const                { return mImplPtr->assets.renderTargetTexture(renderTargetId); }
void Canvas::setRenderTargetClearColor(RenderTargetId renderTargetId, glm::vec4 clearColor) { mImplPtr->assets.setRenderTargetClearColor(renderTargetId, clearColor); }

// ---------------------------------------------------------------------------
// DenseLands — forward to FrameRunner (ExplorerManager<DenseLand> lives there)
// ---------------------------------------------------------------------------

DenseLandId Canvas::addDenseLand(DenseLand denseLand)                                              { return mImplPtr->frameRunner.addDenseLand(std::move(denseLand)); }
void Canvas::removeDenseLand(DenseLandId denseLandId)                                            { mImplPtr->frameRunner.removeDenseLand(denseLandId); }
DenseLand& Canvas::denseLand(DenseLandId denseLandId)                                              { return mImplPtr->frameRunner.denseLand(denseLandId); }
const DenseLand& Canvas::denseLand(DenseLandId denseLandId) const                                  { return mImplPtr->frameRunner.denseLand(denseLandId); }
DenseLandExplorerId Canvas::addDenseLandExplorer(DenseLandExplorer explorer)                     { return mImplPtr->frameRunner.addDenseLandExplorer(explorer, *this); }
void Canvas::removeDenseLandExplorer(DenseLandExplorerId explorerId)                           { mImplPtr->frameRunner.removeDenseLandExplorer(explorerId, *this); }
DenseLandExplorer& Canvas::denseLandExplorer(DenseLandExplorerId explorerId)                     { return mImplPtr->frameRunner.denseLandExplorer(explorerId); }
const DenseLandExplorer& Canvas::denseLandExplorer(DenseLandExplorerId explorerId) const         { return mImplPtr->frameRunner.denseLandExplorer(explorerId); }

// ---------------------------------------------------------------------------
// SparseLands — forward to FrameRunner (ExplorerManager<SparseLand> lives there)
// ---------------------------------------------------------------------------

SparseLandId Canvas::addSparseLand(SparseLand sparseLand)                                              { return mImplPtr->frameRunner.addSparseLand(std::move(sparseLand)); }
void Canvas::removeSparseLand(SparseLandId sparseLandId)                                              { mImplPtr->frameRunner.removeSparseLand(sparseLandId); }
SparseLand& Canvas::sparseLand(SparseLandId sparseLandId)                                              { return mImplPtr->frameRunner.sparseLand(sparseLandId); }
const SparseLand& Canvas::sparseLand(SparseLandId sparseLandId) const                                  { return mImplPtr->frameRunner.sparseLand(sparseLandId); }
SparseLandExplorerId Canvas::addSparseLandExplorer(SparseLandExplorer explorer)                       { return mImplPtr->frameRunner.addSparseLandExplorer(explorer, *this); }
void Canvas::removeSparseLandExplorer(SparseLandExplorerId explorerId)                               { mImplPtr->frameRunner.removeSparseLandExplorer(explorerId, *this); }
SparseLandExplorer& Canvas::sparseLandExplorer(SparseLandExplorerId explorerId)                       { return mImplPtr->frameRunner.sparseLandExplorer(explorerId); }
const SparseLandExplorer& Canvas::sparseLandExplorer(SparseLandExplorerId explorerId) const           { return mImplPtr->frameRunner.sparseLandExplorer(explorerId); }

// ---------------------------------------------------------------------------
// RTT pass scheduling (the queue lives on FrameRunner; the ImGui-to-RTT
// scheduling lives on ImguiRttManager)
// ---------------------------------------------------------------------------

void Canvas::renderTo(RenderTargetId renderTargetId, std::vector<BellotaId> bellotaIds)
{
    mImplPtr->frameRunner.renderTo(renderTargetId, std::move(bellotaIds));
}

void Canvas::renderImguiTo(RenderTargetId renderTargetId, ImguiFontId fontId, ImguiDrawCallback imguiDrawCallback)
{
    // Wrap the user's callback with auto-push/pop of fontId. Graceful fallback:
    // if the bake is still pending or the id was removed, the callback runs
    // without an explicit push (the secondary-context default stays in effect).
    mImplPtr->imguiRtt.enqueue(renderTargetId,
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

// ---------------------------------------------------------------------------
// ImGui fonts — forward to mImplPtr->imguiRtt.fonts() (or wrap in imgui.h calls)
// ---------------------------------------------------------------------------

ImguiFontSourceId Canvas::addImguiFontSource(std::span<const std::byte> ttfBytes, GlyphRange glyphRange) { return mImplPtr->imguiRtt.fonts().addSource(ttfBytes, glyphRange); }
void Canvas::removeImguiFontSource(ImguiFontSourceId sourceId)                                          { mImplPtr->imguiRtt.fonts().removeSource(sourceId); }
ImguiFontSourceId Canvas::defaultImguiFontSourceId() const                                              { return mImplPtr->imguiRtt.fonts().defaultSourceId(); }
ImguiFontSourceId Canvas::boldImguiFontSourceId() const                                                 { return mImplPtr->imguiRtt.fonts().boldSourceId(); }
ImguiFontSourceId Canvas::italicImguiFontSourceId() const                                               { return mImplPtr->imguiRtt.fonts().italicSourceId(); }
ImguiFontSourceId Canvas::boldItalicImguiFontSourceId() const                                           { return mImplPtr->imguiRtt.fonts().boldItalicSourceId(); }
ImguiFontSourceId Canvas::monoImguiFontSourceId() const                                                 { return mImplPtr->imguiRtt.fonts().monoSourceId(); }
std::optional<ImguiFontSourceId> Canvas::embeddedCjkFontSource(CjkScript script) const                  { return mImplPtr->imguiRtt.fonts().cjkSourceId(script); }
ImguiFontId Canvas::bakeImguiFont(ImguiFontSourceId sourceId, float sizePx)                             { return mImplPtr->imguiRtt.fonts().bake(sourceId, sizePx); }
void Canvas::removeImguiFont(ImguiFontId id)                                                            { mImplPtr->imguiRtt.fonts().remove(id); }
bool Canvas::isImguiFontReady(ImguiFontId id) const                                                     { return mImplPtr->imguiRtt.fonts().get(id) != nullptr; }
ImFont* Canvas::getImguiFontPtr(ImguiFontId id) const                                                   { return mImplPtr->imguiRtt.fonts().get(id); }

void Canvas::pushImguiFont(ImguiFontId id)
{
    ImFont* font = getImguiFontPtr(id);
    debugCheck(font != nullptr,
        "Canvas::pushImguiFont: id is not registered or its bake is still pending — guard with isImguiFontReady()");
    ImGui::PushFont(font);
}

void Canvas::popImguiFont()
{
    ImGui::PopFont();
}

ImguiFontId Canvas::defaultImguiFontId() const
{
    auto idOpt = mImplPtr->imguiRtt.fonts().defaultFontId();
    debugCheck(idOpt.has_value(),
        "Canvas::defaultImguiFontId: no default font registered (Canvas ctor seeds this — should never fire)");
    return *idOpt;
}

MarkdownStyle Canvas::defaultMarkdownStyle(float bodySizePx)
{
    // Match the main-canvas UI font recipe (logical point size, see
    // addMainHiDpiFont): ImGui 1.92's dynamic atlas rasterizes at the displayed
    // pixel density, so baking at the raw logical bodySizePx is crisp on HiDPI
    // and sizes markdown like the rest of the main-canvas UI. (The old
    // `* contentScale^2` recipe over-inflated text on HiDPI displays.)
    const float body = bodySizePx;

    MarkdownStyle style;
    style.regular    = bakeImguiFont(defaultImguiFontSourceId(),    body);
    style.bold       = bakeImguiFont(boldImguiFontSourceId(),       body);
    style.italic     = bakeImguiFont(italicImguiFontSourceId(),     body);
    style.boldItalic = bakeImguiFont(boldItalicImguiFontSourceId(), body);
    style.code       = bakeImguiFont(monoImguiFontSourceId(),       body);

    // Headings reuse the Bold face at descending sizes for visual hierarchy.
    const ImguiFontSourceId bold = boldImguiFontSourceId();
    style.headings[0] = bakeImguiFont(bold, body * 1.8f);
    style.headings[1] = bakeImguiFont(bold, body * 1.5f);
    style.headings[2] = bakeImguiFont(bold, body * 1.25f);
    style.headings[3] = bakeImguiFont(bold, body * 1.1f);
    style.headings[4] = bakeImguiFont(bold, body);
    style.headings[5] = bakeImguiFont(bold, body);
    return style;
}

// ---------------------------------------------------------------------------
// Stats flag
// ---------------------------------------------------------------------------

bool& Canvas::stats()                                            { return mImplPtr->frameRunner.stats(); }
const bool& Canvas::stats() const                                { return mImplPtr->frameRunner.stats(); }

// ---------------------------------------------------------------------------
// Lifecycle — thread the assets + imguiRtt managers into the frame loop
// ---------------------------------------------------------------------------

void Canvas::run()
{
    auto update = [](float){};
    Controller controller;
    mImplPtr->frameRunner.run(*this, mImplPtr->assets, mImplPtr->imguiRtt, update, controller);
}

void Canvas::run(std::function<void(float deltaTime)> update)
{
    Controller controller;
    mImplPtr->frameRunner.run(*this, mImplPtr->assets, mImplPtr->imguiRtt, update, controller);
}

void Canvas::run(std::function<void(float deltaTime)> update, Controller& controller)
{
    mImplPtr->frameRunner.run(*this, mImplPtr->assets, mImplPtr->imguiRtt, update, controller);
}

void Canvas::tick(float deltaTime, std::function<void(float)> update, Controller& controller)
{
    mImplPtr->frameRunner.tick(*this, mImplPtr->assets, mImplPtr->imguiRtt, deltaTime, update, controller);
}

void Canvas::tick(float deltaTime, std::function<void(float)> update)
{
    Controller controller;
    mImplPtr->frameRunner.tick(*this, mImplPtr->assets, mImplPtr->imguiRtt, deltaTime, update, controller);
}

void Canvas::tick(float deltaTime)
{
    Controller controller;
    mImplPtr->frameRunner.tick(*this, mImplPtr->assets, mImplPtr->imguiRtt, deltaTime, [](float){}, controller);
}

void Canvas::close()
{
    mImplPtr->frameRunner.close();
}

// ---------------------------------------------------------------------------
// Threaded driver (two-thread sim/render split) — forward to FrameRunner
// ---------------------------------------------------------------------------

void Canvas::beginThreadedSession(Controller& controller)
{
    mImplPtr->frameRunner.beginThreadedSession(controller);
}

bool Canvas::isThreadedRunning() const
{
    return mImplPtr->frameRunner.threadedRunning();
}

void Canvas::commit(float deltaTime, std::function<void(float)> update)
{
    mImplPtr->frameRunner.commitFrame(mImplPtr->assets, deltaTime, std::move(update));
}

void Canvas::renderFrame(Controller& controller)
{
    mImplPtr->frameRunner.renderFrameThreaded(mImplPtr->assets, mImplPtr->imguiRtt, controller);
}

BellotaId Canvas::spawnBellota(const Bellota& bellota)
{
    return mImplPtr->frameRunner.threadedSpawnBellota(mImplPtr->assets, bellota);
}

void Canvas::despawnBellota(BellotaId bellotaId)
{
    mImplPtr->frameRunner.threadedDespawnBellota(mImplPtr->assets, bellotaId);
}

DirectTexture Canvas::takeScreenshot() const
{
    return mImplPtr->frameRunner.takeScreenshot();
}

}
