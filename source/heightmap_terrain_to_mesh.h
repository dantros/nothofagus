#pragma once

#include "mesh3d.h"
#include "../include/heightmap_terrain.h"

namespace Nothofagus
{

/**
 * @brief Generates a 3D triangle mesh from a HeightmapTerrain.
 *
 * Vertices: (x, y, z, u, v) where y = heights[row*columns+col] * maximumHeight.
 * UV: (u, v) = (col / (columns-1), row / (rows-1)).
 * Triangles: 2 CCW triangles per grid cell.
 */
Mesh3D generateHeightmapTerrainMesh(const HeightmapTerrain& terrain);

} // namespace Nothofagus
