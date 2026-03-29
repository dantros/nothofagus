#include "heightmap_terrain_to_mesh.h"
#include "check.h"

namespace Nothofagus
{

Mesh3D generateHeightmapTerrainMesh(const Bellota& bellota, const DirectTexture& heightTexture)
{
    const std::size_t columns      = static_cast<std::size_t>(heightTexture.size().x);
    const std::size_t rows         = static_cast<std::size_t>(heightTexture.size().y);
    const glm::vec3   scale        = bellota.transform3d().scale();
    const float       worldWidth   = scale.x;
    const float       maximumHeight = scale.y;
    const float       worldDepth   = scale.z;

    debugCheck(rows >= 2,    "HeightmapTerrain requires at least 2 rows");
    debugCheck(columns >= 2, "HeightmapTerrain requires at least 2 columns");

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
            const float y = heightTexture.getFloat(col, row) * maximumHeight;
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
