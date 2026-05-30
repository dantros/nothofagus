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
 * @struct Canvas::Private
 * @brief Single nested pimpl holding every internal collaborator. Owning a
 *        single Private struct lets `canvas.h` expose only this one nested
 *        forward declaration — `Nothofagus::AssetRegistry` and
 *        `Nothofagus::ImguiRttManager` never appear in the public header.
 *
 * Member declaration order matches construction order; reverse-destruction
 * matches the GPU teardown sequence (ImGui RTT contexts → asset GPU handles
 * → ~CanvasImpl runs mBackend.shutdown()).
 */
struct Canvas::Private
{
    std::unique_ptr<CanvasImpl>       canvasImpl;
    std::unique_ptr<AssetRegistry>    assets;
    std::unique_ptr<ImguiRttManager>  imguiRtt;
};

Canvas::Canvas(
    const ScreenSize& screenSize,
    const std::string& title,
    const glm::vec3 clearColor,
    const unsigned int pixelSize,
    const float imguiFontSize,
    bool headless
)
    : mPrivate(std::make_unique<Private>())
{
    // Construction order matters: CanvasImpl builds mBackend and initializes
    // the GPU + ImGui main context. AssetRegistry takes a reference to that
    // backend. ImguiRttManager takes references to the backend AND the
    // registry's RenderTargetContainer. Finally bake the main HiDPI font
    // (needs the backend's ImGui renderer to be live).
    mPrivate->canvasImpl = std::make_unique<CanvasImpl>(screenSize, title, clearColor, pixelSize, headless);
    mPrivate->assets     = std::make_unique<AssetRegistry>(mPrivate->canvasImpl->backend());
    mPrivate->imguiRtt   = std::make_unique<ImguiRttManager>(
        mPrivate->canvasImpl->backend(),
        mPrivate->assets->renderTargets(),
        assets_Roboto_VariableFont_wdth_wght_ttf,
        assets_Roboto_VariableFont_wdth_wght_ttf_len,
        imguiFontSize);
    mPrivate->imguiRtt->fonts().initialize(mPrivate->canvasImpl->contentScale());
}

Canvas::~Canvas()
{
    // GPU resources must be freed while the backend is still alive. Drain
    // imguiRtt and assets explicitly here; the unique_ptrs inside Private
    // then destroy in reverse declaration order (imguiRtt → assets →
    // canvasImpl, where the last one shuts the backend down).
    mPrivate->imguiRtt->releaseAll();
    mPrivate->assets->freeAllGpuResources();
}

// ---------------------------------------------------------------------------
// Window / display — forward to CanvasImpl
// ---------------------------------------------------------------------------

std::size_t Canvas::getCurrentMonitor() const                   { return mPrivate->canvasImpl->getCurrentMonitor(); }
bool Canvas::isFullscreen() const                               { return mPrivate->canvasImpl->isFullscreen(); }
void Canvas::setFullScreenOnMonitor(std::size_t monitor)        { mPrivate->canvasImpl->setFullScreenOnMonitor(monitor); }
void Canvas::setWindowed()                                       { mPrivate->canvasImpl->setWindowed(); }
const ScreenSize& Canvas::screenSize() const                    { return mPrivate->canvasImpl->screenSize(); }
void Canvas::setScreenSize(const ScreenSize& screenSize)        { mPrivate->canvasImpl->setScreenSize(screenSize); }
void Canvas::setClearColor(glm::vec3 clearColor)                { mPrivate->canvasImpl->setClearColor(clearColor); }
void Canvas::setAutoRemoveUnusedTextures(bool enabled)          { mPrivate->canvasImpl->setAutoRemoveUnusedTextures(enabled); }
void Canvas::setAutoRemoveUnusedMeshes(bool enabled)            { mPrivate->canvasImpl->setAutoRemoveUnusedMeshes(enabled); }
void Canvas::setWindowTitle(const std::string& title)           { mPrivate->canvasImpl->setWindowTitle(title); }
ScreenSize Canvas::windowSize() const                            { return mPrivate->canvasImpl->windowSize(); }
ViewportRect Canvas::gameViewport() const                       { return mPrivate->canvasImpl->gameViewport(); }

// ---------------------------------------------------------------------------
// Bellotas — forward to AssetRegistry; remove gates against TilemapExplorer pool
// ---------------------------------------------------------------------------

BellotaId Canvas::addBellota(const Bellota& bellota)            { return mPrivate->assets->addBellota(bellota); }

void Canvas::removeBellota(const BellotaId bellotaId)
{
    debugCheck(!mPrivate->canvasImpl->isExplorerManagedBellota(bellotaId.id),
        "Bellota is owned by a TilemapExplorer pool — use canvas.removeTilemapExplorer() instead of removing slot bellotas directly.");
    mPrivate->assets->removeBellota(bellotaId);
}

Bellota& Canvas::bellota(BellotaId bellotaId)                   { return mPrivate->assets->bellota(bellotaId); }
const Bellota& Canvas::bellota(BellotaId bellotaId) const       { return mPrivate->assets->bellota(bellotaId); }
void Canvas::setTint(const BellotaId bellotaId, const Tint& tint) { mPrivate->assets->setTint(bellotaId, tint); }
void Canvas::removeTint(const BellotaId bellotaId)              { mPrivate->assets->removeTint(bellotaId); }

// ---------------------------------------------------------------------------
// Textures — forward to AssetRegistry; remove gates against TilemapExplorer pool
// ---------------------------------------------------------------------------

TextureId Canvas::addTexture(const Texture& texture)            { return mPrivate->assets->addTexture(texture); }

void Canvas::removeTexture(const TextureId textureId)
{
    debugCheck(!mPrivate->canvasImpl->isExplorerManagedTexture(textureId.id),
        "Texture is owned by a TilemapExplorer pool — use canvas.removeTilemapExplorer() instead of removing slot textures directly.");
    mPrivate->assets->removeTexture(textureId);
}

void Canvas::setTexture(const BellotaId bellotaId, const TextureId textureId)            { mPrivate->assets->setTexture(bellotaId, textureId); }
void Canvas::markTextureAsDirty(const TextureId textureId)                                { mPrivate->assets->markTextureAsDirty(textureId); }
void Canvas::setTextureMinFilter(const TextureId textureId, TextureSampleMode mode)       { mPrivate->assets->setTextureMinFilter(textureId, mode); }
void Canvas::setTextureMagFilter(const TextureId textureId, TextureSampleMode mode)       { mPrivate->assets->setTextureMagFilter(textureId, mode); }
Texture& Canvas::texture(TextureId textureId)                                             { return mPrivate->assets->texture(textureId); }
const Texture& Canvas::texture(TextureId textureId) const                                 { return mPrivate->assets->texture(textureId); }

// ---------------------------------------------------------------------------
// Meshes — forward to AssetRegistry
// ---------------------------------------------------------------------------

MeshId Canvas::addMesh(const Mesh& mesh)                                                  { return mPrivate->assets->addMesh(mesh); }
MeshId Canvas::addMesh(Mesh&& mesh)                                                       { return mPrivate->assets->addMesh(std::move(mesh)); }
void Canvas::removeMesh(MeshId meshId)                                                    { mPrivate->assets->removeMesh(meshId); }
void Canvas::setMesh(const BellotaId bellotaId, const MeshId meshId)                      { mPrivate->assets->setMesh(bellotaId, meshId); }
const Mesh& Canvas::mesh(MeshId meshId) const                                             { return mPrivate->assets->mesh(meshId); }
const Mesh& Canvas::mesh(BellotaId bellotaId) const                                       { return mPrivate->assets->mesh(bellotaId); }

// ---------------------------------------------------------------------------
// Render targets — forward to AssetRegistry; remove tears down the per-RTT
// ImGui secondary context first (its backend resources are tied to the RTT
// render pass / FBO that the registry is about to free).
// ---------------------------------------------------------------------------

RenderTargetId Canvas::addRenderTarget(ScreenSize size)                                   { return mPrivate->assets->addRenderTarget(size); }

void Canvas::removeRenderTarget(RenderTargetId renderTargetId)
{
    mPrivate->imguiRtt->releaseContext(renderTargetId);
    mPrivate->assets->removeRenderTarget(renderTargetId);
}

TextureId Canvas::renderTargetTexture(RenderTargetId renderTargetId) const                { return mPrivate->assets->renderTargetTexture(renderTargetId); }
void Canvas::setRenderTargetClearColor(RenderTargetId renderTargetId, glm::vec4 clearColor) { mPrivate->assets->setRenderTargetClearColor(renderTargetId, clearColor); }

// ---------------------------------------------------------------------------
// Tilemaps — forward to CanvasImpl (TilemapManager lives there)
// ---------------------------------------------------------------------------

TilemapId Canvas::addTilemap(Tilemap tilemap)                                              { return mPrivate->canvasImpl->addTilemap(std::move(tilemap)); }
void Canvas::removeTilemap(TilemapId tilemapId)                                            { mPrivate->canvasImpl->removeTilemap(tilemapId); }
Tilemap& Canvas::tilemap(TilemapId tilemapId)                                              { return mPrivate->canvasImpl->tilemap(tilemapId); }
const Tilemap& Canvas::tilemap(TilemapId tilemapId) const                                  { return mPrivate->canvasImpl->tilemap(tilemapId); }
TilemapExplorerId Canvas::addTilemapExplorer(TilemapExplorer explorer)                     { return mPrivate->canvasImpl->addTilemapExplorer(explorer, *this); }
void Canvas::removeTilemapExplorer(TilemapExplorerId explorerId)                           { mPrivate->canvasImpl->removeTilemapExplorer(explorerId, *this); }
TilemapExplorer& Canvas::tilemapExplorer(TilemapExplorerId explorerId)                     { return mPrivate->canvasImpl->tilemapExplorer(explorerId); }
const TilemapExplorer& Canvas::tilemapExplorer(TilemapExplorerId explorerId) const         { return mPrivate->canvasImpl->tilemapExplorer(explorerId); }

// ---------------------------------------------------------------------------
// RTT pass scheduling (the queue lives on CanvasImpl; the ImGui-to-RTT
// scheduling lives on ImguiRttManager)
// ---------------------------------------------------------------------------

void Canvas::renderTo(RenderTargetId renderTargetId, std::vector<BellotaId> bellotaIds)
{
    mPrivate->canvasImpl->renderTo(renderTargetId, std::move(bellotaIds));
}

void Canvas::renderImguiTo(RenderTargetId renderTargetId, ImguiFontId fontId, ImguiDrawCallback imguiDrawCallback)
{
    // Wrap the user's callback with auto-push/pop of fontId. Graceful fallback:
    // if the bake is still pending or the id was removed, the callback runs
    // without an explicit push (the secondary-context default stays in effect).
    mPrivate->imguiRtt->enqueue(renderTargetId,
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
// ImGui fonts — forward to mPrivate->imguiRtt->fonts() (or wrap in imgui.h calls)
// ---------------------------------------------------------------------------

ImguiFontSourceId Canvas::addImguiFontSource(std::span<const std::byte> ttfBytes, GlyphRange glyphRange) { return mPrivate->imguiRtt->fonts().addSource(ttfBytes, glyphRange); }
void Canvas::removeImguiFontSource(ImguiFontSourceId sourceId)                                          { mPrivate->imguiRtt->fonts().removeSource(sourceId); }
ImguiFontSourceId Canvas::defaultImguiFontSourceId() const                                              { return mPrivate->imguiRtt->fonts().defaultSourceId(); }
ImguiFontId Canvas::bakeImguiFont(ImguiFontSourceId sourceId, float sizePx)                             { return mPrivate->imguiRtt->fonts().bake(sourceId, sizePx); }
void Canvas::removeImguiFont(ImguiFontId id)                                                            { mPrivate->imguiRtt->fonts().remove(id); }
bool Canvas::isImguiFontReady(ImguiFontId id) const                                                     { return mPrivate->imguiRtt->fonts().get(id) != nullptr; }
ImFont* Canvas::getImguiFontPtr(ImguiFontId id) const                                                   { return mPrivate->imguiRtt->fonts().get(id); }

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
    auto idOpt = mPrivate->imguiRtt->fonts().defaultFontId();
    debugCheck(idOpt.has_value(),
        "Canvas::defaultImguiFontId: no default font registered (Canvas ctor seeds this — should never fire)");
    return *idOpt;
}

// ---------------------------------------------------------------------------
// Stats flag
// ---------------------------------------------------------------------------

bool& Canvas::stats()                                            { return mPrivate->canvasImpl->stats(); }
const bool& Canvas::stats() const                                { return mPrivate->canvasImpl->stats(); }

// ---------------------------------------------------------------------------
// Lifecycle — thread the assets + imguiRtt managers into the frame loop
// ---------------------------------------------------------------------------

void Canvas::run()
{
    auto update = [](float){};
    Controller controller;
    mPrivate->canvasImpl->run(*this, *mPrivate->assets, *mPrivate->imguiRtt, update, controller);
}

void Canvas::run(std::function<void(float deltaTime)> update)
{
    Controller controller;
    mPrivate->canvasImpl->run(*this, *mPrivate->assets, *mPrivate->imguiRtt, update, controller);
}

void Canvas::run(std::function<void(float deltaTime)> update, Controller& controller)
{
    mPrivate->canvasImpl->run(*this, *mPrivate->assets, *mPrivate->imguiRtt, update, controller);
}

void Canvas::tick(float deltaTime, std::function<void(float)> update, Controller& controller)
{
    mPrivate->canvasImpl->tick(*this, *mPrivate->assets, *mPrivate->imguiRtt, deltaTime, update, controller);
}

void Canvas::tick(float deltaTime, std::function<void(float)> update)
{
    Controller controller;
    mPrivate->canvasImpl->tick(*this, *mPrivate->assets, *mPrivate->imguiRtt, deltaTime, update, controller);
}

void Canvas::tick(float deltaTime)
{
    Controller controller;
    mPrivate->canvasImpl->tick(*this, *mPrivate->assets, *mPrivate->imguiRtt, deltaTime, [](float){}, controller);
}

void Canvas::close()
{
    mPrivate->canvasImpl->close();
}

DirectTexture Canvas::takeScreenshot() const
{
    return mPrivate->canvasImpl->takeScreenshot();
}

}
