#pragma once

#include "mesh3d.h"

namespace Nothofagus
{

/**
 * @struct DMesh3D
 * @brief GPU-resident 3D mesh: VAO/VBO/EBO for (x,y,z,u,v) interleaved vertices.
 *
 * Vertex layout: position (3 floats) + textureCoordinates (2 floats) = 5 floats per vertex.
 * Attribute names in shaders: "position" and "textureCoordinates".
 */
struct DMesh3D
{
    unsigned int vao        = 0;
    unsigned int vbo        = 0;
    unsigned int ebo        = 0;
    unsigned int texture    = 0; ///< GL_TEXTURE_2D_ARRAY handle (set after init)
    std::size_t  indexCount = 0;

    /**
     * @brief Allocates VAO, VBO, and EBO on the GPU.
     */
    void initBuffers();

    /**
     * @brief Uploads vertex and index data to the GPU.
     * @param mesh   The 3D mesh data (vertices + indices).
     * @param usage  OpenGL usage hint (e.g. GL_STATIC_DRAW or GL_DYNAMIC_DRAW).
     */
    void fillBuffers(const Mesh3D& mesh, unsigned int usage);

    /**
     * @brief Binds this mesh's VAO, binds the texture, issues glDrawElements, then unbinds.
     */
    void drawCall() const;

    /**
     * @brief Releases VAO, VBO, and EBO GPU memory.
     */
    void clear() const;
};

} // namespace Nothofagus
