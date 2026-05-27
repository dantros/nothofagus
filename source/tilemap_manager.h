#pragma once

#include "tilemap.h"
#include "tilemap_view.h"
#include "tilemap_view_pack.h"
#include "indexed_container.h"
#include <cstddef>
#include <unordered_set>

namespace Nothofagus
{

class Canvas;

/// Storage and per-frame logic for huge tilemaps: the `Tilemap` registry, the
/// `TilemapView` pool packs, and the view-managed tag sets that police user-side
/// bellota/texture removals. Three methods that need to touch canvas-owned
/// bellotas/textures take a `Canvas&` and use only its public surface.
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
    /// registered through `canvas.addTexture` / `canvas.addBellota`
    /// and tagged view-managed.
    TilemapViewId addTilemapView(TilemapView view, Canvas& canvas);

    /// Untags + removes every pool slot's bellota and texture via
    /// `canvas.removeBellota` / `canvas.removeTexture`, then drops the view pack.
    void          removeTilemapView(TilemapViewId id, Canvas& canvas);

    TilemapView&       tilemapView(TilemapViewId id);
    const TilemapView& tilemapView(TilemapViewId id) const;

    // ── Per-frame pre-pass (needs canvas access to mutate slot bellotas + textures) ─
    /// Runs in `Canvas::CanvasImpl::runOneFrame` between the user update and
    /// the texture upload pass. For each view, assigns visible world chunks
    /// to pool slots, memcpys chunk data into the slot's IndirectTexture
    /// via `setMapBulk`, and repositions/un-hides the slot bellota.
    void updateViews(Canvas& canvas);

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
