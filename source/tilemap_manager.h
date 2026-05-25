#pragma once

#include "canvas.h"
#include "tilemap.h"
#include "tilemap_view.h"
#include "tilemap_view_pack.h"
#include "indexed_container.h"
#include <cstddef>
#include <unordered_set>

namespace Nothofagus
{

/**
 * @class TilemapManager
 * @brief Owns the data + per-frame logic for huge tilemaps: the `Tilemap`
 *        registry, the `TilemapView` pool packs, and the view-managed tag
 *        sets consulted by `Canvas::CanvasImpl` when policing user-side
 *        bellota/texture removals.
 *
 * The manager intentionally holds no reference to `Canvas::CanvasImpl`.
 * The three methods that need to touch canvas-owned bellotas/textures
 * (`addTilemapView`, `removeTilemapView`, `updateViews`) take it as an
 * explicit argument. The pure-storage methods (`addTilemap`, accessors,
 * predicates) don't.
 */
class TilemapManager
{
public:
    TilemapManager() = default;

    // ── Tilemap (pure data storage) ───────────────────────────────────────
    TilemapId      addTilemap(Tilemap tilemap);
    void           removeTilemap(TilemapId id);                ///< debugCheck: no view references it.
    Tilemap&       tilemap(TilemapId id);
    const Tilemap& tilemap(TilemapId id) const;

    // ── TilemapView lifecycle (need canvas access for pool init/teardown) ─
    /// Allocates the pool: one `IndirectTexture` + one `Bellota` per slot,
    /// registered through `canvasImpl.addTexture` / `canvasImpl.addBellota`
    /// and tagged view-managed.
    TilemapViewId addTilemapView(TilemapView view, Canvas::CanvasImpl& canvasImpl);

    /// Untags + removes every pool slot's bellota and texture via
    /// `canvasImpl.removeBellota` / `canvasImpl.removeTexture`, then drops
    /// the view pack.
    void          removeTilemapView(TilemapViewId id, Canvas::CanvasImpl& canvasImpl);

    TilemapView&       tilemapView(TilemapViewId id);
    const TilemapView& tilemapView(TilemapViewId id) const;

    // ── Per-frame pre-pass (needs canvas access to mutate slot bellotas + textures) ─
    /// Runs in `Canvas::CanvasImpl::runOneFrame` between the user update and
    /// the texture upload pass. For each view, assigns visible world chunks
    /// to pool slots, memcpys chunk data into the slot's IndirectTexture
    /// via `setMapBulk`, and repositions/un-hides the slot bellota.
    void updateViews(Canvas::CanvasImpl& canvasImpl);

    // ── View-managed predicates (consulted by removeBellota / removeTexture) ─
    bool isViewManagedBellota(std::size_t bellotaId) const
        { return mViewManagedBellotaIds.contains(bellotaId); }
    bool isViewManagedTexture(std::size_t textureId) const
        { return mViewManagedTextureIds.contains(textureId); }

    std::size_t tilemapCount() const { return mTilemaps.size(); }
    std::size_t viewCount() const    { return mTilemapViews.size(); }

private:
    IndexedContainer<Tilemap>       mTilemaps;
    TilemapViewContainer            mTilemapViews;
    std::unordered_set<std::size_t> mViewManagedBellotaIds;
    std::unordered_set<std::size_t> mViewManagedTextureIds;
};

}
