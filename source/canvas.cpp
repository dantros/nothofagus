#include "canvas.h"
#include "canvas_impl.h"
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
 *        one nested forward declaration — `Nothofagus::CanvasImpl`,
 *        `Nothofagus::AssetRegistry`, and `Nothofagus::ImguiRttManager` never
 *        appear in the public header.
 *
 * Member declaration order = construction order:
 *   `canvasImpl` first (builds mBackend, creates window, init ImGui)
 *   → `assets` (binds to canvasImpl.backend())
 *   → `imguiRtt` (binds to canvasImpl.backend() and assets.renderTargets()).
 *
 * Reverse-destruction matches the GPU teardown sequence (ImGui RTT contexts
 * → asset GPU handles → ~CanvasImpl runs mBackend.shutdown()).
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
        : canvasImpl(screenSize, title, clearColor, pixelSize, headless),
          assets(canvasImpl.backend()),
          imguiRtt(canvasImpl.backend(), assets.renderTargets(),
                   assets_Roboto_VariableFont_wdth_wght_ttf,
                   assets_Roboto_VariableFont_wdth_wght_ttf_len,
                   imguiFontSize)
    {
        // Main HiDPI font bake — needs the backend's ImGui renderer to be live,
        // which it is once CanvasImpl's ctor has returned.
        imguiRtt.fonts().initialize(canvasImpl.contentScale());
    }

    CanvasImpl       canvasImpl;
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
    : mImplementation(std::make_unique<Implementation>(
          screenSize, title, clearColor, pixelSize, imguiFontSize, headless))
{
}

Canvas::~Canvas()
{
    // GPU resources must be freed while the backend is still alive. Drain
    // imguiRtt and assets explicitly here; the by-value members inside
    // Implementation then destroy in reverse declaration order
    // (imguiRtt → assets → canvasImpl, where the last one shuts the backend down).
    mImplementation->imguiRtt.releaseAll();
    mImplementation->assets.freeAllGpuResources();
}

// ---------------------------------------------------------------------------
// Window / display — forward to CanvasImpl
// ---------------------------------------------------------------------------

std::size_t Canvas::getCurrentMonitor() const                   { return mImplementation->canvasImpl.getCurrentMonitor(); }
bool Canvas::isFullscreen() const                               { return mImplementation->canvasImpl.isFullscreen(); }
void Canvas::setFullScreenOnMonitor(std::size_t monitor)        { mImplementation->canvasImpl.setFullScreenOnMonitor(monitor); }
void Canvas::setWindowed()                                       { mImplementation->canvasImpl.setWindowed(); }
const ScreenSize& Canvas::screenSize() const                    { return mImplementation->canvasImpl.screenSize(); }
void Canvas::setScreenSize(const ScreenSize& screenSize)        { mImplementation->canvasImpl.setScreenSize(screenSize); }
void Canvas::setClearColor(glm::vec3 clearColor)                { mImplementation->canvasImpl.setClearColor(clearColor); }
void Canvas::setAutoRemoveUnusedTextures(bool enabled)          { mImplementation->canvasImpl.setAutoRemoveUnusedTextures(enabled); }
void Canvas::setAutoRemoveUnusedMeshes(bool enabled)            { mImplementation->canvasImpl.setAutoRemoveUnusedMeshes(enabled); }
void Canvas::setWindowTitle(const std::string& title)           { mImplementation->canvasImpl.setWindowTitle(title); }
ScreenSize Canvas::windowSize() const                            { return mImplementation->canvasImpl.windowSize(); }
ViewportRect Canvas::gameViewport() const                       { return mImplementation->canvasImpl.gameViewport(); }

// ---------------------------------------------------------------------------
// Bellotas — forward to AssetRegistry; remove gates against TilemapExplorer pool
// ---------------------------------------------------------------------------

BellotaId Canvas::addBellota(const Bellota& bellota)            { return mImplementation->assets.addBellota(bellota); }

void Canvas::removeBellota(const BellotaId bellotaId)
{
    debugCheck(!mImplementation->canvasImpl.isExplorerManagedBellota(bellotaId.id),
        "Bellota is owned by a TilemapExplorer pool — use canvas.removeTilemapExplorer() instead of removing slot bellotas directly.");
    mImplementation->assets.removeBellota(bellotaId);
}

Bellota& Canvas::bellota(BellotaId bellotaId)                   { return mImplementation->assets.bellota(bellotaId); }
const Bellota& Canvas::bellota(BellotaId bellotaId) const       { return mImplementation->assets.bellota(bellotaId); }
void Canvas::setTint(const BellotaId bellotaId, const Tint& tint) { mImplementation->assets.setTint(bellotaId, tint); }
void Canvas::removeTint(const BellotaId bellotaId)              { mImplementation->assets.removeTint(bellotaId); }

// ---------------------------------------------------------------------------
// Textures — forward to AssetRegistry; remove gates against TilemapExplorer pool
// ---------------------------------------------------------------------------

TextureId Canvas::addTexture(const Texture& texture)            { return mImplementation->assets.addTexture(texture); }

void Canvas::removeTexture(const TextureId textureId)
{
    debugCheck(!mImplementation->canvasImpl.isExplorerManagedTexture(textureId.id),
        "Texture is owned by a TilemapExplorer pool — use canvas.removeTilemapExplorer() instead of removing slot textures directly.");
    mImplementation->assets.removeTexture(textureId);
}

void Canvas::setTexture(const BellotaId bellotaId, const TextureId textureId)            { mImplementation->assets.setTexture(bellotaId, textureId); }
void Canvas::markTextureAsDirty(const TextureId textureId)                                { mImplementation->assets.markTextureAsDirty(textureId); }
void Canvas::setTextureMinFilter(const TextureId textureId, TextureSampleMode mode)       { mImplementation->assets.setTextureMinFilter(textureId, mode); }
void Canvas::setTextureMagFilter(const TextureId textureId, TextureSampleMode mode)       { mImplementation->assets.setTextureMagFilter(textureId, mode); }
Texture& Canvas::texture(TextureId textureId)                                             { return mImplementation->assets.texture(textureId); }
const Texture& Canvas::texture(TextureId textureId) const                                 { return mImplementation->assets.texture(textureId); }

// ---------------------------------------------------------------------------
// Meshes — forward to AssetRegistry
// ---------------------------------------------------------------------------

MeshId Canvas::addMesh(const Mesh& mesh)                                                  { return mImplementation->assets.addMesh(mesh); }
MeshId Canvas::addMesh(Mesh&& mesh)                                                       { return mImplementation->assets.addMesh(std::move(mesh)); }
void Canvas::removeMesh(MeshId meshId)                                                    { mImplementation->assets.removeMesh(meshId); }
void Canvas::setMesh(const BellotaId bellotaId, const MeshId meshId)                      { mImplementation->assets.setMesh(bellotaId, meshId); }
const Mesh& Canvas::mesh(MeshId meshId) const                                             { return mImplementation->assets.mesh(meshId); }
const Mesh& Canvas::mesh(BellotaId bellotaId) const                                       { return mImplementation->assets.mesh(bellotaId); }

// ---------------------------------------------------------------------------
// Render targets — forward to AssetRegistry; remove tears down the per-RTT
// ImGui secondary context first (its backend resources are tied to the RTT
// render pass / FBO that the registry is about to free).
// ---------------------------------------------------------------------------

RenderTargetId Canvas::addRenderTarget(ScreenSize size)                                   { return mImplementation->assets.addRenderTarget(size); }

void Canvas::removeRenderTarget(RenderTargetId renderTargetId)
{
    mImplementation->imguiRtt.releaseContext(renderTargetId);
    mImplementation->assets.removeRenderTarget(renderTargetId);
}

TextureId Canvas::renderTargetTexture(RenderTargetId renderTargetId) const                { return mImplementation->assets.renderTargetTexture(renderTargetId); }
void Canvas::setRenderTargetClearColor(RenderTargetId renderTargetId, glm::vec4 clearColor) { mImplementation->assets.setRenderTargetClearColor(renderTargetId, clearColor); }

// ---------------------------------------------------------------------------
// Tilemaps — forward to CanvasImpl (TilemapManager lives there)
// ---------------------------------------------------------------------------

TilemapId Canvas::addTilemap(Tilemap tilemap)                                              { return mImplementation->canvasImpl.addTilemap(std::move(tilemap)); }
void Canvas::removeTilemap(TilemapId tilemapId)                                            { mImplementation->canvasImpl.removeTilemap(tilemapId); }
Tilemap& Canvas::tilemap(TilemapId tilemapId)                                              { return mImplementation->canvasImpl.tilemap(tilemapId); }
const Tilemap& Canvas::tilemap(TilemapId tilemapId) const                                  { return mImplementation->canvasImpl.tilemap(tilemapId); }
TilemapExplorerId Canvas::addTilemapExplorer(TilemapExplorer explorer)                     { return mImplementation->canvasImpl.addTilemapExplorer(explorer, *this); }
void Canvas::removeTilemapExplorer(TilemapExplorerId explorerId)                           { mImplementation->canvasImpl.removeTilemapExplorer(explorerId, *this); }
TilemapExplorer& Canvas::tilemapExplorer(TilemapExplorerId explorerId)                     { return mImplementation->canvasImpl.tilemapExplorer(explorerId); }
const TilemapExplorer& Canvas::tilemapExplorer(TilemapExplorerId explorerId) const         { return mImplementation->canvasImpl.tilemapExplorer(explorerId); }

// ---------------------------------------------------------------------------
// RTT pass scheduling (the queue lives on CanvasImpl; the ImGui-to-RTT
// scheduling lives on ImguiRttManager)
// ---------------------------------------------------------------------------

void Canvas::renderTo(RenderTargetId renderTargetId, std::vector<BellotaId> bellotaIds)
{
    mImplementation->canvasImpl.renderTo(renderTargetId, std::move(bellotaIds));
}

void Canvas::renderImguiTo(RenderTargetId renderTargetId, ImguiFontId fontId, ImguiDrawCallback imguiDrawCallback)
{
    // Wrap the user's callback with auto-push/pop of fontId. Graceful fallback:
    // if the bake is still pending or the id was removed, the callback runs
    // without an explicit push (the secondary-context default stays in effect).
    mImplementation->imguiRtt.enqueue(renderTargetId,
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
// ImGui fonts — forward to mImplementation->imguiRtt.fonts() (or wrap in imgui.h calls)
// ---------------------------------------------------------------------------

ImguiFontSourceId Canvas::addImguiFontSource(std::span<const std::byte> ttfBytes, GlyphRange glyphRange) { return mImplementation->imguiRtt.fonts().addSource(ttfBytes, glyphRange); }
void Canvas::removeImguiFontSource(ImguiFontSourceId sourceId)                                          { mImplementation->imguiRtt.fonts().removeSource(sourceId); }
ImguiFontSourceId Canvas::defaultImguiFontSourceId() const                                              { return mImplementation->imguiRtt.fonts().defaultSourceId(); }
ImguiFontId Canvas::bakeImguiFont(ImguiFontSourceId sourceId, float sizePx)                             { return mImplementation->imguiRtt.fonts().bake(sourceId, sizePx); }
void Canvas::removeImguiFont(ImguiFontId id)                                                            { mImplementation->imguiRtt.fonts().remove(id); }
bool Canvas::isImguiFontReady(ImguiFontId id) const                                                     { return mImplementation->imguiRtt.fonts().get(id) != nullptr; }
ImFont* Canvas::getImguiFontPtr(ImguiFontId id) const                                                   { return mImplementation->imguiRtt.fonts().get(id); }

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
    auto idOpt = mImplementation->imguiRtt.fonts().defaultFontId();
    debugCheck(idOpt.has_value(),
        "Canvas::defaultImguiFontId: no default font registered (Canvas ctor seeds this — should never fire)");
    return *idOpt;
}

// ---------------------------------------------------------------------------
// Stats flag
// ---------------------------------------------------------------------------

bool& Canvas::stats()                                            { return mImplementation->canvasImpl.stats(); }
const bool& Canvas::stats() const                                { return mImplementation->canvasImpl.stats(); }

// ---------------------------------------------------------------------------
// Lifecycle — thread the assets + imguiRtt managers into the frame loop
// ---------------------------------------------------------------------------

void Canvas::run()
{
    auto update = [](float){};
    Controller controller;
    mImplementation->canvasImpl.run(*this, mImplementation->assets, mImplementation->imguiRtt, update, controller);
}

void Canvas::run(std::function<void(float deltaTime)> update)
{
    Controller controller;
    mImplementation->canvasImpl.run(*this, mImplementation->assets, mImplementation->imguiRtt, update, controller);
}

void Canvas::run(std::function<void(float deltaTime)> update, Controller& controller)
{
    mImplementation->canvasImpl.run(*this, mImplementation->assets, mImplementation->imguiRtt, update, controller);
}

void Canvas::tick(float deltaTime, std::function<void(float)> update, Controller& controller)
{
    mImplementation->canvasImpl.tick(*this, mImplementation->assets, mImplementation->imguiRtt, deltaTime, update, controller);
}

void Canvas::tick(float deltaTime, std::function<void(float)> update)
{
    Controller controller;
    mImplementation->canvasImpl.tick(*this, mImplementation->assets, mImplementation->imguiRtt, deltaTime, update, controller);
}

void Canvas::tick(float deltaTime)
{
    Controller controller;
    mImplementation->canvasImpl.tick(*this, mImplementation->assets, mImplementation->imguiRtt, deltaTime, [](float){}, controller);
}

void Canvas::close()
{
    mImplementation->canvasImpl.close();
}

DirectTexture Canvas::takeScreenshot() const
{
    return mImplementation->canvasImpl.takeScreenshot();
}

}
