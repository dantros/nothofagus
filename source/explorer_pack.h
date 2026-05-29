#pragma once

#include "explorer.h"
#include "bellota.h"
#include "indexed_container.h"
#include "screen_size.h"
#include <glm/glm.hpp>
#include <cstdint>
#include <vector>

namespace Nothofagus
{

/// One slot in an `Explorer<T>`'s chunk pool — IDs are stable for the slot's lifetime.
/// Backend-agnostic: a slot doesn't know whether it serves a `Tilemap` or a `Sparsemap`,
/// it just paints whatever chunk the manager assigns to it via `setMapBulk`.
struct PoolSlot
{
    TextureId     textureId;
    BellotaId     bellotaId;
    glm::ivec2    currentWorldChunk{-1, -1}; ///< World chunk currently painted; {-1,-1} = unassigned.
    std::uint64_t syncedGeneration{0};       ///< Backend chunk generation last written into this slot.
};

template<TilemapLike T>
struct ExplorerPack
{
    Explorer<T>               explorer;
    glm::ivec2                poolGridSize{0, 0};
    ScreenSize                poolSizedFor{0, 0}; ///< Canvas screenSize the current pool was sized for; drives re-allocation in updateExplorers.
    std::vector<PoolSlot>     slots;
    std::vector<std::uint8_t> chunkScratch; ///< Reused per re-sync; resized to `chunkSize.x * chunkSize.y` when the pool is (re)built.

    explicit ExplorerPack(Explorer<T> v): explorer(v) {}
};

}
