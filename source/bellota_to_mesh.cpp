#include "bellota_to_mesh.h"
#include "dvertex.h"
#include "check.h"
#include <glm/glm.hpp>

namespace Nothofagus
{

/**
 * @brief Retrieves the size of the texture for a given Bellota object from the TextureContainer.
 * 
 * @param textures The TextureContainer holding all textures.
 * @param bellota The Bellota object whose texture size is to be retrieved.
 * @return The size of the texture (width and height) for the given Bellota.
 */
glm::ivec2 getTextureSize(const TextureContainer& textures, const Bellota& bellota)
{
    const TextureId& textureId = bellota.texture();
    const TexturePack& texturePack = textures.at(textureId.id);
    const Texture& texture = texturePack.texture;
    return texture.size();
}

/**
 * @brief Retrieves the size of the texture for a given AnimatedBellota object from the TextureArrayContainer.
 * 
 * @param texturesArrays The TextureArrayContainer holding all texture arrays.
 * @param animatedBellota The AnimatedBellota object whose texture array size is to be retrieved.
 * @return The size of the texture array (width and height) for the given AnimatedBellota.
 */
glm::ivec2 getTextureSize(const TextureArrayContainer& texturesArrays, const AnimatedBellota& animatedBellota)
{
    const TextureId& textureId = animatedBellota.texture();
    const TextureArrayPack& textureArrayPack = texturesArrays.at(textureId.id);
    const TextureArray& textureArray = textureArrayPack.textureArray;
    return textureArray.size();
}

/**
 * @brief Creates a DVertex object corresponding to the bottom-left corner of the texture.
 * 
 * @param size The size of the texture (width and height).
 * @return The DVertex for the bottom-left corner of the texture.
 */
DVertex bottomLeft(const glm::ivec2 size)
{
    return { size.x / -2.0f, size.y / -2.0f, 0, 1};
}

/**
 * @brief Creates a DVertex object corresponding to the bottom-right corner of the texture.
 * 
 * @param size The size of the texture (width and height).
 * @return The DVertex for the bottom-right corner of the texture.
 */
DVertex bottomRight(const glm::ivec2 size)
{
    return { size.x / 2.0f, size.y / -2.0f, 1, 1};
}

/**
 * @brief Creates a DVertex object corresponding to the upper-left corner of the texture.
 * 
 * @param size The size of the texture (width and height).
 * @return The DVertex for the upper-left corner of the texture.
 */
DVertex upperLeft(const glm::ivec2 size)
{
    return { size.x / -2.0f, size.y / 2.0f, 0, 0};
}

/**
 * @brief Creates a DVertex object corresponding to the upper-right corner of the texture.
 * 
 * @param size The size of the texture (width and height).
 * @return The DVertex for the upper-right corner of the texture.
 */
DVertex upperRight(const glm::ivec2 size)
{
    return { size.x / 2.0f, size.y / 2.0f, 1, 0};
}

Mesh generateMesh2(const glm::ivec2 &size)
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

/**
 * @brief Generates a mesh for a Bellota object, using the textures provided by the TextureContainer.
 *
 * This function generates a 2D mesh with texture coordinates, vertices, and indices for a Bellota object.
 *
 * @param textures The TextureContainer holding the textures to be used for generating the mesh.
 * @param bellota The Bellota object for which the mesh is to be generated.
 * @return The generated mesh.
 */
Mesh generateMesh(const TextureContainer& textures, const Bellota& bellota)
{
    const glm::ivec2 size = getTextureSize(textures, bellota);
    return generateMesh2(size);
}

/**
 * @brief Generates a mesh for an AnimatedBellota object, using the textures provided by the TextureArrayContainer.
 * 
 * This function generates a 2D mesh with texture coordinates, vertices, and indices for an AnimatedBellota object.
 * 
 * @param texturesArrays The TextureArrayContainer holding the texture arrays to be used for generating the mesh.
 * @param animatedBellota The AnimatedBellota object for which the mesh is to be generated.
 * @return The generated mesh.
 */
Mesh generateMesh(const TextureArrayContainer& texturesArrays, const AnimatedBellota& animatedBellota)
{
    const glm::ivec2 size = getTextureSize(texturesArrays, animatedBellota);
    return generateMesh2(size);
}

}