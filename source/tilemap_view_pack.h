#pragma once

#include "tilemap_view.h"
#include "bellota.h"
#include "indexed_container.h"
#include <glm/glm.hpp>
#include <cstdint>
#include <vector>

namespace Nothofagus
{

/// One slot in a TilemapView's chunk pool — IDs are stable for the slot's lifetime.
struct PoolSlot
{
    TextureId     textureId;
    BellotaId     bellotaId;
    glm::ivec2    currentWorldChunk{-1, -1}; ///< World chunk currently painted; {-1,-1} = unassigned.
    std::uint64_t syncedGeneration{0};       ///< Tilemap chunk generation last written into this slot.
};

struct TilemapViewPack
{
    TilemapView           view;
    glm::ivec2            poolGridSize{0, 0};
    std::vector<PoolSlot> slots;

    explicit TilemapViewPack(TilemapView v): view(v) {}
};

using TilemapViewContainer = IndexedContainer<TilemapViewPack>;

}
