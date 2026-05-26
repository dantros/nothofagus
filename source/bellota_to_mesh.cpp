#include "bellota_to_mesh.h"

namespace Nothofagus
{

namespace
{
    Vertex bottomLeft(const glm::ivec2 size)  { return { size.x / -2.0f, size.y / -2.0f, 0.0f, 1.0f }; }
    Vertex bottomRight(const glm::ivec2 size) { return { size.x /  2.0f, size.y / -2.0f, 1.0f, 1.0f }; }
    Vertex upperLeft(const glm::ivec2 size)   { return { size.x / -2.0f, size.y /  2.0f, 0.0f, 0.0f }; }
    Vertex upperRight(const glm::ivec2 size)  { return { size.x /  2.0f, size.y /  2.0f, 1.0f, 0.0f }; }
}

Mesh generateQuadMesh(const glm::ivec2& size)
{
    Mesh mesh;

    mesh.vertices.reserve(4);
    mesh.vertices.push_back(bottomLeft(size));   // 0
    mesh.vertices.push_back(bottomRight(size));  // 1
    mesh.vertices.push_back(upperRight(size));   // 2
    mesh.vertices.push_back(upperLeft(size));    // 3

    mesh.indices.reserve(6);
    // Bottom Right Triangle
    mesh.indices.push_back(0);
    mesh.indices.push_back(1);
    mesh.indices.push_back(2);
    // Upper Left Triangle
    mesh.indices.push_back(2);
    mesh.indices.push_back(3);
    mesh.indices.push_back(0);

    return mesh;
}

}
