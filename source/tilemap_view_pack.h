#pragma once

#include "tilemap_view.h"
#include "bellota.h"
#include "indexed_container.h"
#include <glm/glm.hpp>
#include <cstdint>
#include <vector>

namespace Nothofagus
{

/// One slot in a TilemapView's chunk pool. The (textureId, bellotaId)
/// pair is allocated once at pool init and reused for the slot's lifetime;
/// `currentWorldChunk` records which world chunk the slot is currently
/// painting and `syncedGeneration` tracks the Tilemap chunk generation
/// last written into the slot's IndirectTexture.
struct PoolSlot
{
    TextureId     textureId;
    BellotaId     bellotaId;
    glm::ivec2    currentWorldChunk{-1, -1}; ///< {-1,-1} = unassigned
    std::uint64_t syncedGeneration{0};
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
