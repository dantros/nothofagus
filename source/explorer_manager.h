#pragma once

#include "explorer.h"
#include "explorer_pack.h"
#include "indexed_container.h"
#include "screen_size.h"
#include <cstddef>
#include <unordered_set>
#include <glm/glm.hpp>

namespace Nothofagus
{

class Canvas;

/// Storage and per-frame logic for a single `LandType` backend: the data registry,
/// the explorer pool packs, and the explorer-managed tag sets that police user-side
/// bellota/texture removals. Three methods that need to touch canvas-owned
/// bellotas/textures take a `Canvas&` and use only its public surface.
///
/// Instantiated once per backend in `CanvasImpl`: `ExplorerManager<DenseLand>` for the
/// dense huge-world path, `ExplorerManager<SparseLand>` for the sparse / streaming
/// path. The two managers are independent — their explorer-managed sets do not overlap.
template<LandType T>
class ExplorerManager
{
public:
    using LandId     = typename LandTraits<T>::LandId;
    using ExplorerId = typename LandTraits<T>::ExplorerId;

    ExplorerManager() = default;

    /// Defensive cleanup of host-side bookkeeping. Pool slot bellotas / textures
    /// belong to the canvas's asset registry and are released by the registry's
    /// own teardown; this dtor never calls `teardownPoolSlots` because it would
    /// need a `Canvas&` it doesn't hold. Future destruction-order refactors that
    /// destroy this manager before the asset registry can rely on these clears
    /// keeping host state consistent up to the point GPU teardown finishes.
    ~ExplorerManager()
    {
        mExplorerManagedBellotaIds.clear();
        mExplorerManagedTextureIds.clear();
    }

    // ── Data (pure storage) ───────────────────────────────────────────────
    LandId   add(T data);
    void     remove(LandId id);                ///< debugCheck: no explorer references it.
    T&       get(LandId id);
    const T& get(LandId id) const;

    // ── Explorer lifecycle (need canvas access for pool init/teardown) ─
    /// Allocates the pool: one `IndirectTexture` + one `Bellota` per slot,
    /// registered through `canvas.addTexture` / `canvas.addBellota`
    /// and tagged explorer-managed.
    ExplorerId addExplorer(Explorer<T> explorer, Canvas& canvas);

    /// Untags + removes every pool slot's bellota and texture via
    /// `canvas.removeBellota` / `canvas.removeTexture`, then drops the explorer pack.
    void removeExplorer(ExplorerId id, Canvas& canvas);

    Explorer<T>&       getExplorer(ExplorerId id);
    const Explorer<T>& getExplorer(ExplorerId id) const;

    // ── Per-frame pre-pass (needs canvas access to mutate slot bellotas + textures) ─
    /// Runs in `Canvas::CanvasImpl::runOneFrame` between the user update and
    /// the texture upload pass. Iterates each explorer pack and delegates the
    /// per-explorer work to `updateExplorer`, which in turn dispatches each pool
    /// slot to the file-local `exploreCell` helper.
    void updateExplorers(Canvas& canvas);

    // ── Explorer-managed predicates (consulted by removeBellota / removeTexture) ─
    bool isExplorerManagedBellota(std::size_t bellotaId) const
        { return mExplorerManagedBellotaIds.contains(bellotaId); }
    bool isExplorerManagedTexture(std::size_t textureId) const
        { return mExplorerManagedTextureIds.contains(textureId); }

    std::size_t dataCount()     const { return mData.size(); }
    std::size_t explorerCount() const { return mExplorers.size(); }

private:
    /// Allocates / resizes one explorer's pool against the current `canvas.screenSize()`.
    /// Used both at registration time and by `updateExplorers` when the canvas size changes.
    void buildPoolSlots(ExplorerPack<T>& pack, Canvas& canvas);

    /// Tears down every slot's bellota + texture (untag, then canvas remove) and
    /// clears `pack.slots`. Used at removal time and at the head of `buildPoolSlots`'s
    /// re-allocation path.
    void teardownPoolSlots(ExplorerPack<T>& pack, Canvas& canvas);

    /// Per-frame work for a single explorer: resize the pool if the canvas size
    /// changed, compute the visible chunk window, then sync each pool slot via
    /// the file-local `exploreCell` helper.
    void updateExplorer(
        ExplorerPack<T>& explorerPack,
        Canvas& canvas,
        const ScreenSize& screen,
        const glm::vec2& canvasCenter);

    IndexedContainer<T>                  mData;
    IndexedContainer<ExplorerPack<T>>    mExplorers;
    std::unordered_set<std::size_t>      mExplorerManagedBellotaIds;
    std::unordered_set<std::size_t>      mExplorerManagedTextureIds;
};

}
