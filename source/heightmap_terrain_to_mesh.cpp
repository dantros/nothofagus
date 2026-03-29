#include "heightmap_terrain_to_mesh.h"
#include "check.h"

namespace Nothofagus
{

Mesh3D generateHeightmapTerrainMesh(const HeightmapTerrain& terrain)
{
    const std::size_t rows         = terrain.rows();
    const std::size_t columns      = terrain.columns();
    const float       worldWidth   = terrain.worldWidth();
    const float       worldDepth   = terrain.worldDepth();
    const float       maximumHeight = terrain.maximumHeight();
    const auto&       heights      = terrain.heights();

    debugCheck(rows >= 2,    "HeightmapTerrain requires at least 2 rows");
    debugCheck(columns >= 2, "HeightmapTerrain requires at least 2 columns");
    debugCheck(heights.size() == rows * columns,
               "HeightmapTerrain heights size must equal rows * columns");

    Mesh3D mesh;
    mesh.vertices.reserve(rows * columns * 5);
    mesh.indices.reserve((rows - 1) * (columns - 1) * 6);

    // Vertices: x, y, z, u, v
    for (std::size_t row = 0; row < rows; ++row)
    {
        for (std::size_t col = 0; col < columns; ++col)
        {
            const float x = static_cast<float>(col) / static_cast<float>(columns - 1) * worldWidth;
            const float z = static_cast<float>(row) / static_cast<float>(rows    - 1) * worldDepth;
            const float y = heights[row * columns + col] * maximumHeight;
            const float u = static_cast<float>(col) / static_cast<float>(columns - 1);
            const float v = static_cast<float>(row) / static_cast<float>(rows    - 1);

            mesh.vertices.push_back(x);
            mesh.vertices.push_back(y);
            mesh.vertices.push_back(z);
            mesh.vertices.push_back(u);
            mesh.vertices.push_back(v);
        }
    }

    // Indices: two CCW triangles per grid cell
    for (std::size_t row = 0; row < rows - 1; ++row)
    {
        for (std::size_t col = 0; col < columns - 1; ++col)
        {
            const unsigned int topLeft     = static_cast<unsigned int>(row       * columns + col);
            const unsigned int topRight    = static_cast<unsigned int>(row       * columns + col + 1);
            const unsigned int bottomLeft  = static_cast<unsigned int>((row + 1) * columns + col);
            const unsigned int bottomRight = static_cast<unsigned int>((row + 1) * columns + col + 1);

            // Triangle 1
            mesh.indices.push_back(topLeft);
            mesh.indices.push_back(bottomLeft);
            mesh.indices.push_back(bottomRight);

            // Triangle 2
            mesh.indices.push_back(topLeft);
            mesh.indices.push_back(bottomRight);
            mesh.indices.push_back(topRight);
        }
    }

    return mesh;
}

} // namespace Nothofagus
