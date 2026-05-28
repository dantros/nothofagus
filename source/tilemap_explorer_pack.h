#pragma once

#include "tilemap_explorer.h"
#include "bellota.h"
#include "indexed_container.h"
#include "screen_size.h"
#include <glm/glm.hpp>
#include <cstdint>
#include <vector>

namespace Nothofagus
{

/// One slot in a TilemapExplorer's chunk pool — IDs are stable for the slot's lifetime.
struct PoolSlot
{
    TextureId     textureId;
    BellotaId     bellotaId;
    glm::ivec2    currentWorldChunk{-1, -1}; ///< World chunk currently painted; {-1,-1} = unassigned.
    std::uint64_t syncedGeneration{0};       ///< Tilemap chunk generation last written into this slot.
};

struct TilemapExplorerPack
{
    TilemapExplorer               explorer;
    glm::ivec2                poolGridSize{0, 0};
    ScreenSize                poolSizedFor{0, 0}; ///< Canvas screenSize the current pool was sized for; drives re-allocation in updateExplorers.
    std::vector<PoolSlot>     slots;
    std::vector<std::uint8_t> chunkScratch; ///< Reused per re-sync; resized to `chunkSize.x * chunkSize.y` when the pool is (re)built.

    explicit TilemapExplorerPack(TilemapExplorer v): explorer(v) {}
};

using TilemapExplorerContainer = IndexedContainer<TilemapExplorerPack>;

}
