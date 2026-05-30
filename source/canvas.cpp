#include "canvas.h"
#include "frame_runner.h"
#include "asset_registry.h"
#include "imgui_rtt_manager.h"
#include "check.h"
#include "roboto_font.h"
#include <imgui.h>

namespace Nothofagus
{

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
                   assets_Roboto_VariableFont_wdth_wght_ttf,
                   assets_Roboto_VariableFont_wdth_wght_ttf_len,
                   imguiFontSize)
    {
        // Main HiDPI font bake — needs the backend's ImGui renderer to be live,
        // which it is once FrameRunner's ctor has returned.
        imguiRtt.fonts().initialize(frameRunner.contentScale());
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

// ---------------------------------------------------------------------------
// Bellotas — forward to AssetRegistry; remove gates against TilemapExplorer pool
// ---------------------------------------------------------------------------

BellotaId Canvas::addBellota(const Bellota& bellota)            { return mImplPtr->assets.addBellota(bellota); }

void Canvas::removeBellota(const BellotaId bellotaId)
{
    debugCheck(!mImplPtr->frameRunner.isExplorerManagedBellota(bellotaId.id),
        "Bellota is owned by a TilemapExplorer pool — use canvas.removeTilemapExplorer() instead of removing slot bellotas directly.");
    mImplPtr->assets.removeBellota(bellotaId);
}

Bellota& Canvas::bellota(BellotaId bellotaId)                   { return mImplPtr->assets.bellota(bellotaId); }
const Bellota& Canvas::bellota(BellotaId bellotaId) const       { return mImplPtr->assets.bellota(bellotaId); }
void Canvas::setTint(const BellotaId bellotaId, const Tint& tint) { mImplPtr->assets.setTint(bellotaId, tint); }
void Canvas::removeTint(const BellotaId bellotaId)              { mImplPtr->assets.removeTint(bellotaId); }

// ---------------------------------------------------------------------------
// Textures — forward to AssetRegistry; remove gates against TilemapExplorer pool
// ---------------------------------------------------------------------------

TextureId Canvas::addTexture(const Texture& texture)            { return mImplPtr->assets.addTexture(texture); }

void Canvas::removeTexture(const TextureId textureId)
{
    debugCheck(!mImplPtr->frameRunner.isExplorerManagedTexture(textureId.id),
        "Texture is owned by a TilemapExplorer pool — use canvas.removeTilemapExplorer() instead of removing slot textures directly.");
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
// Tilemaps — forward to FrameRunner (TilemapManager lives there)
// ---------------------------------------------------------------------------

TilemapId Canvas::addTilemap(Tilemap tilemap)                                              { return mImplPtr->frameRunner.addTilemap(std::move(tilemap)); }
void Canvas::removeTilemap(TilemapId tilemapId)                                            { mImplPtr->frameRunner.removeTilemap(tilemapId); }
Tilemap& Canvas::tilemap(TilemapId tilemapId)                                              { return mImplPtr->frameRunner.tilemap(tilemapId); }
const Tilemap& Canvas::tilemap(TilemapId tilemapId) const                                  { return mImplPtr->frameRunner.tilemap(tilemapId); }
TilemapExplorerId Canvas::addTilemapExplorer(TilemapExplorer explorer)                     { return mImplPtr->frameRunner.addTilemapExplorer(explorer, *this); }
void Canvas::removeTilemapExplorer(TilemapExplorerId explorerId)                           { mImplPtr->frameRunner.removeTilemapExplorer(explorerId, *this); }
TilemapExplorer& Canvas::tilemapExplorer(TilemapExplorerId explorerId)                     { return mImplPtr->frameRunner.tilemapExplorer(explorerId); }
const TilemapExplorer& Canvas::tilemapExplorer(TilemapExplorerId explorerId) const         { return mImplPtr->frameRunner.tilemapExplorer(explorerId); }

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

DirectTexture Canvas::takeScreenshot() const
{
    return mImplPtr->frameRunner.takeScreenshot();
}

}
