#pragma once

#include "tilemap.h"
#include "tilemap_explorer.h"
#include "tilemap_explorer_pack.h"
#include "indexed_container.h"
#include <cstddef>
#include <unordered_set>

namespace Nothofagus
{

class Canvas;

/// Storage and per-frame logic for huge tilemaps: the `Tilemap` registry, the
/// `TilemapExplorer` pool packs, and the explorer-managed tag sets that police user-side
/// bellota/texture removals. Three methods that need to touch canvas-owned
/// bellotas/textures take a `Canvas&` and use only its public surface.
class TilemapManager
{
public:
    TilemapManager() = default;

    // ── Tilemap (pure data storage) ───────────────────────────────────────
    TilemapId      addTilemap(Tilemap tilemap);
    void           removeTilemap(TilemapId id);                ///< debugCheck: no explorer references it.
    Tilemap&       tilemap(TilemapId id);
    const Tilemap& tilemap(TilemapId id) const;

    // ── TilemapExplorer lifecycle (need canvas access for pool init/teardown) ─
    /// Allocates the pool: one `IndirectTexture` + one `Bellota` per slot,
    /// registered through `canvas.addTexture` / `canvas.addBellota`
    /// and tagged explorer-managed.
    TilemapExplorerId addTilemapExplorer(TilemapExplorer explorer, Canvas& canvas);

    /// Untags + removes every pool slot's bellota and texture via
    /// `canvas.removeBellota` / `canvas.removeTexture`, then drops the explorer pack.
    void          removeTilemapExplorer(TilemapExplorerId id, Canvas& canvas);

    TilemapExplorer&       tilemapExplorer(TilemapExplorerId id);
    const TilemapExplorer& tilemapExplorer(TilemapExplorerId id) const;

    // ── Per-frame pre-pass (needs canvas access to mutate slot bellotas + textures) ─
    /// Runs in `Canvas::CanvasImpl::runOneFrame` between the user update and
    /// the texture upload pass. For each explorer, assigns visible world chunks
    /// to pool slots, memcpys chunk data into the slot's IndirectTexture
    /// via `setMapBulk`, and repositions/un-hides the slot bellota.
    void updateExplorers(Canvas& canvas);

    // ── View-managed predicates (consulted by removeBellota / removeTexture) ─
    bool isExplorerManagedBellota(std::size_t bellotaId) const
        { return mExplorerManagedBellotaIds.contains(bellotaId); }
    bool isExplorerManagedTexture(std::size_t textureId) const
        { return mExplorerManagedTextureIds.contains(textureId); }

    std::size_t tilemapCount() const { return mTilemaps.size(); }
    std::size_t explorerCount() const    { return mTilemapExplorers.size(); }

private:
    /// Allocates / resizes one explorer's pool against the current `canvas.screenSize()`.
    /// Used both at registration time and by `updateExplorers` when the canvas size changes.
    void buildPoolSlots(TilemapExplorerPack& pack, Canvas& canvas);

    /// Tears down every slot's bellota + texture (untag, then canvas remove) and
    /// clears `pack.slots`. Used at removal time and at the head of `buildPoolSlots`'s
    /// re-allocation path.
    void teardownPoolSlots(TilemapExplorerPack& pack, Canvas& canvas);

    IndexedContainer<Tilemap>       mTilemaps;
    TilemapExplorerContainer            mTilemapExplorers;
    std::unordered_set<std::size_t> mExplorerManagedBellotaIds;
    std::unordered_set<std::size_t> mExplorerManagedTextureIds;
};

}
