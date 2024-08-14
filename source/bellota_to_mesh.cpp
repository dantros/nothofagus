#include "bellota_to_mesh.h"
#include "dvertex.h"
#include "check.h"
#include <glm/glm.hpp>

namespace Nothofagus
{

glm::ivec2 getTextureSize(const TextureContainer& textures, const Bellota& bellota)
{
    const TextureId& textureId = bellota.texture();
    const Texture& texture = textures.at(textureId);
    return texture.size();
}

DVertex bottomLeft(const glm::ivec2 size)
{
    return { size.x / -2.0f, size.y / -2.0f, 0, 1};
}

DVertex bottomRight(const glm::ivec2 size)
{
    return { size.x / 2.0f, size.y / -2.0f, 1, 1};
}

DVertex upperLeft(const glm::ivec2 size)
{
    return { size.x / -2.0f, size.y / 2.0f, 0, 0};
}

DVertex upperRight(const glm::ivec2 size)
{
    return { size.x / 2.0f, size.y / 2.0f, 1, 0};
}

Mesh generateMesh(const TextureContainer& textures, const Bellota& bellota)
{
    Mesh mesh;

    mesh.vertices.reserve(16);

    auto addVertex = [&mesh](const DVertex& vertex)
    {
        mesh.vertices.push_back(vertex.x);
        mesh.vertices.push_back(vertex.y);
        mesh.vertices.push_back(vertex.tx);
        mesh.vertices.push_back(vertex.ty);
    };

    const glm::ivec2 size = getTextureSize(textures, bellota)
    
    addVertex(bottomLeft(size));  // vertex 0
    addVertex(bottomRight(size)); // vertex 1
    addVertex(upperRight(size));  // vertex 2
    addVertex(upperLeft(size));   // vertex 3

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