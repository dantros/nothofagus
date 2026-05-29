#pragma once

#include "canvas.h"
#include "tilemap.h"
#include "tilemap_explorer.h"
#include "tilemap_manager.h"
#include "asset_registry.h"
#include "imgui_rtt_manager.h"
#include "imgui_font_source_id.h"
#include "aa_box.h"
#include "backends/render_backend_select.h"
#include <vector>
#include <utility>
#include <span>
#include <cstddef>

struct ImFont;

namespace Nothofagus
{

/**
 * @class Canvas::CanvasImpl
 * @brief Implementation of the Canvas class, responsible for managing the actual window, textures, Bellotas, and rendering.
 *
 * Encapsulates the low-level details of the Canvas: window/input backend,
 * GPU rendering backend, asset containers, the frame loop, and screenshot
 * capture. Most asset/state accessors are inline forwarders onto the
 * `mAssets` (AssetRegistry), `mTilemapManager`, and `mImguiRtt.fonts()`
 * members. The out-of-line definitions in canvas_impl.cpp are the ones
 * that (a) depend on the pimpl-hidden `Window` type, (b) need imgui.h or
 * carry non-trivial multi-statement logic, or (c) layer a cross-cutting
 * gate (tilemap-pool ownership, ImGui RTT teardown) on top of an asset
 * forwarder.
 */
class Canvas::CanvasImpl
{
public:

    CanvasImpl(
        const ScreenSize& screenSize,
        const std::string& title,
        const glm::vec3 clearColor,
        const unsigned int pixelSize,
        const float imguiFontSize,
        bool headless = false
    );

    /// Destructor to clean up resources and terminate the window backend
    ~CanvasImpl();

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
    const ScreenSize& screenSize() const                                                    { return mScreenSize; }
    void setScreenSize(const ScreenSize& screenSize)                                        { mScreenSize = screenSize; }
    void setClearColor(glm::vec3 clearColor)                                                { mClearColor = clearColor; }
    ViewportRect gameViewport() const                                                       { return mGameViewport; }
    bool& stats()                                                                           { return mStats; }
    const bool& stats() const                                                               { return mStats; }
    void setAutoRemoveUnusedTextures(bool enabled)                                          { mAutoTextureGC = enabled; }
    void setAutoRemoveUnusedMeshes(bool enabled)                                            { mAutoMeshGC = enabled; }

    // ----- Bellotas -----
    BellotaId addBellota(const Bellota& bellota)                                            { return mAssets.addBellota(bellota); }
    void removeBellota(const BellotaId bellotaId);     // tilemap-pool ownership gate — defined in cpp
    Bellota& bellota(BellotaId bellotaId)                                                   { return mAssets.bellota(bellotaId); }
    const Bellota& bellota(BellotaId bellotaId) const                                       { return mAssets.bellota(bellotaId); }
    void setTint(const BellotaId bellotaId, const Tint& tint)                               { mAssets.setTint(bellotaId, tint); }
    void removeTint(const BellotaId bellotaId)                                              { mAssets.removeTint(bellotaId); }

    // ----- Textures -----
    TextureId addTexture(const Texture& texture)                                            { return mAssets.addTexture(texture); }
    void removeTexture(const TextureId textureId);     // tilemap-pool ownership gate — defined in cpp
    void setTexture(const BellotaId bellotaId, const TextureId textureId)                   { mAssets.setTexture(bellotaId, textureId); }
    void markTextureAsDirty(const TextureId textureId)                                      { mAssets.markTextureAsDirty(textureId); }
    void setTextureMinFilter(const TextureId textureId, TextureSampleMode mode)             { mAssets.setTextureMinFilter(textureId, mode); }
    void setTextureMagFilter(const TextureId textureId, TextureSampleMode mode)             { mAssets.setTextureMagFilter(textureId, mode); }
    Texture& texture(TextureId textureId)                                                   { return mAssets.texture(textureId); }
    const Texture& texture(TextureId textureId) const                                       { return mAssets.texture(textureId); }
    Texture& textureArray(TextureId textureId);                                             // legacy declaration — no definition; calling it is a link error
    const Texture& textureArray(TextureId textureId) const;                                 // legacy declaration — no definition; calling it is a link error

    // ----- Meshes -----
    MeshId addMesh(const Mesh& mesh)                                                        { return mAssets.addMesh(mesh); }
    MeshId addMesh(Mesh&& mesh)                                                             { return mAssets.addMesh(std::move(mesh)); }
    void removeMesh(MeshId meshId)                                                          { mAssets.removeMesh(meshId); }
    void setMesh(const BellotaId bellotaId, const MeshId meshId)                            { mAssets.setMesh(bellotaId, meshId); }
    const Mesh& mesh(MeshId meshId) const                                                   { return mAssets.mesh(meshId); }
    const Mesh& mesh(BellotaId bellotaId) const                                             { return mAssets.mesh(bellotaId); }

    // ----- Render targets -----
    RenderTargetId addRenderTarget(ScreenSize size)                                         { return mAssets.addRenderTarget(size); }
    void removeRenderTarget(RenderTargetId renderTargetId);    // ImGui RTT context teardown first — defined in cpp
    TextureId renderTargetTexture(RenderTargetId renderTargetId) const                      { return mAssets.renderTargetTexture(renderTargetId); }
    void setRenderTargetClearColor(RenderTargetId renderTargetId, glm::vec4 clearColor)     { mAssets.setRenderTargetClearColor(renderTargetId, clearColor); }

    // ----- Tilemaps -----
    TilemapId addTilemap(Tilemap tilemap)                                                   { return mTilemapManager.addTilemap(std::move(tilemap)); }
    void removeTilemap(TilemapId tilemapId)                                                 { mTilemapManager.removeTilemap(tilemapId); }
    Tilemap& tilemap(TilemapId tilemapId)                                                   { return mTilemapManager.tilemap(tilemapId); }
    const Tilemap& tilemap(TilemapId tilemapId) const                                       { return mTilemapManager.tilemap(tilemapId); }
    TilemapExplorerId addTilemapExplorer(TilemapExplorer explorer, Canvas& canvas)          { return mTilemapManager.addTilemapExplorer(explorer, canvas); }
    void removeTilemapExplorer(TilemapExplorerId explorerId, Canvas& canvas)                { mTilemapManager.removeTilemapExplorer(explorerId, canvas); }
    TilemapExplorer& tilemapExplorer(TilemapExplorerId explorerId)                          { return mTilemapManager.tilemapExplorer(explorerId); }
    const TilemapExplorer& tilemapExplorer(TilemapExplorerId explorerId) const              { return mTilemapManager.tilemapExplorer(explorerId); }

    // ----- RTT pass scheduling -----
    void renderTo(RenderTargetId renderTargetId, std::vector<BellotaId> bellotaIds)         { mPendingRttPasses.emplace_back(renderTargetId, std::move(bellotaIds)); }
    void renderImguiTo(RenderTargetId renderTargetId, ImguiFontId fontId, ImguiDrawCallback imguiDrawCallback);

    // ----- ImGui fonts -----
    ImguiFontSourceId addImguiFontSource(std::span<const std::byte> ttfBytes, GlyphRange glyphRange) { return mImguiRtt.fonts().addSource(ttfBytes, glyphRange); }
    void removeImguiFontSource(ImguiFontSourceId sourceId)                                           { mImguiRtt.fonts().removeSource(sourceId); }
    ImguiFontSourceId defaultImguiFontSourceId() const                                               { return mImguiRtt.fonts().defaultSourceId(); }
    ImguiFontId bakeImguiFont(ImguiFontSourceId sourceId, float sizePx)                              { return mImguiRtt.fonts().bake(sourceId, sizePx); }
    void removeImguiFont(ImguiFontId id)                                                             { mImguiRtt.fonts().remove(id); }
    bool isImguiFontReady(ImguiFontId id) const                                                      { return mImguiRtt.fonts().get(id) != nullptr; }
    ImFont* getImguiFontPtr(ImguiFontId id) const                                                    { return mImguiRtt.fonts().get(id); }
    void pushImguiFont(ImguiFontId id);            // needs imgui.h + debugCheck on optional — defined in cpp
    void popImguiFont();                            // needs imgui.h — defined in cpp
    ImguiFontId defaultImguiFontId() const;         // debugCheck on optional — defined in cpp

    // ----- Lifecycle -----
    /// Runs the main loop of the canvas with a custom update function.
    /// @param canvas Reference to the owning Canvas, threaded through to TilemapManager.
    void run(Canvas& canvas, std::function<void(float deltaTime)> update, Controller& controller);

    /// Execute a single frame with a caller-supplied delta time (in milliseconds).
    void tick(Canvas& canvas, float deltaTimeMS, std::function<void(float)> update, Controller& controller);
    void tick(Canvas& canvas, float deltaTimeMS, std::function<void(float)> update);
    void tick(Canvas& canvas, float deltaTimeMS);

    /// Captures the last rendered frame visible to the user as a DirectTexture (RGBA).
    DirectTexture takeScreenshot() const;

private:
    void ensureSessionStarted(Controller& controller);
    void runOneFrame(Canvas& canvas, float deltaTimeMS, std::function<void(float)> update, Controller& controller);

    ScreenSize mScreenSize; ///< The screen size of the canvas.
    std::string mTitle; ///< The title of the canvas window.
    glm::vec3 mClearColor; ///< The background color of the canvas.
    unsigned int mPixelSize; ///< The pixel size on the canvas.

    ActiveBackend mBackend; ///< GPU rendering backend (compile-time selected).

    /// Owns the four CPU-side asset containers (textures, bellotas, meshes,
    /// render targets) and the two usage monitors. Holds a reference to
    /// mBackend so per-asset removes can free GPU resources eagerly;
    /// bulk teardown goes through mAssets.freeAllGpuResources() in the dtor.
    /// Declared after mBackend so the reference is bound to a live backend.
    AssetRegistry mAssets;

    TilemapManager mTilemapManager; ///< Huge-tilemap storage + per-frame explorer pool logic.

    /// RTT passes queued by renderTo() during the update callback, executed before the main render.
    std::vector<std::pair<RenderTargetId, std::vector<BellotaId>>> mPendingRttPasses;

    /// Owns per-RTT secondary ImGuiContexts + the per-frame RTT pass queue,
    /// AND the canvas-wide ImGui font manager (main HiDPI font + RTT default
    /// + user-baked sizes; deferred bake/remove queue + atlas rebuild).
    /// Declared after mBackend / mAssets so initialization order is well-defined.
    ImguiRttManager mImguiRtt;

    bool mStats; ///< Flag to indicate whether stats should be displayed.
    bool mHeadless{false}; ///< When true, the window is hidden (no visible UI).
    bool mSessionStarted{false}; ///< True after ensureSessionStarted() has been called.
    bool mAutoTextureGC{true}; ///< When true, unreferenced textures are removed each frame.
    bool mAutoMeshGC{true};    ///< When true, unreferenced meshes are removed each frame.
    std::vector<const BellotaPack*> mSortedBellotaPacks; ///< Reusable depth-sorted draw list.

    struct Window; ///< Forward declaration for window management.
    std::unique_ptr<Window> mWindow; ///< Pointer to the window object.

    AABox mLastWindowedAABox;
    ViewportRect mGameViewport; ///< Current letterboxed game viewport (set each frame in run()).
};

}
