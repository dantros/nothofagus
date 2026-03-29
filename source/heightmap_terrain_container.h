#pragma once

#include "../include/bellota.h"
#include "mesh3d.h"
#include "dmesh3d.h"
#include "indexed_container.h"
#include <optional>

namespace Nothofagus
{

/**
 * @struct HeightmapTerrainPack
 * @brief Links a canvas-registered Bellota (Transform3D for world dims) to a height DirectTexture,
 *        plus the lazy-initialized CPU/GPU meshes.
 *
 * The bellota's Transform3D scale encodes world dimensions:
 *   scale.x = worldWidth, scale.y = maximumHeight, scale.z = worldDepth
 * The height texture (DirectTexture) stores one float per pixel (via getFloat/setFloat).
 * Rows = heightTexture.size().y, columns = heightTexture.size().x.
 */
struct HeightmapTerrainPack
{
    BellotaId             bellotaId;
    TextureId             heightTextureId;
    std::optional<Mesh3D>  meshOpt;
    std::optional<DMesh3D> dmeshOpt;

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
