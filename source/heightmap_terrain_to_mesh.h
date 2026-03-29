#pragma once

#include "mesh3d.h"
#include "../include/bellota.h"
#include "../include/texture.h"

namespace Nothofagus
{

/**
 * @brief Generates a 3D triangle mesh from a terrain bellota and its height DirectTexture.
 *
 * The bellota's Transform3D scale encodes world dimensions:
 *   scale.x = worldWidth, scale.y = maximumHeight, scale.z = worldDepth
 * The height texture stores one float per pixel (via getFloat):
 *   columns = heightTexture.size().x, rows = heightTexture.size().y
 *
 * Vertices: (x, y, z, u, v) where y = heightTexture.getFloat(col, row) * maximumHeight.
 * UV: (u, v) = (col / (columns-1), row / (rows-1)).
 * Triangles: 2 CCW triangles per grid cell.
 */
Mesh3D generateHeightmapTerrainMesh(const Bellota& bellota, const DirectTexture& heightTexture);

} // namespace Nothofagus
