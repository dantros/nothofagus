#pragma once

#include "../include/heightmap_terrain.h"
#include "mesh3d.h"
#include "dmesh3d.h"
#include "indexed_container.h"
#include <optional>

namespace Nothofagus
{

/**
 * @struct HeightmapTerrainPack
 * @brief Stores a HeightmapTerrain alongside its optional CPU mesh and GPU mesh.
 *
 * Follows the same dirty-flag lazy-init pattern as BellotaPack.
 */
struct HeightmapTerrainPack
{
    HeightmapTerrain        terrain;
    std::optional<Mesh3D>   meshOpt;
    std::optional<DMesh3D>  dmeshOpt;

    bool isDirty() const
    {
        return not (meshOpt.has_value() and dmeshOpt.has_value());
    }

    void clear()
    {
        meshOpt.reset();
        if (dmeshOpt.has_value())
        {
            dmeshOpt.value().clear();
            dmeshOpt.reset();
        }
    }
};

using HeightmapTerrainContainer = IndexedContainer<HeightmapTerrainPack>;

} // namespace Nothofagus
