#pragma once

#include "mesh.h"
#include <iostream>
//#include "shaders_core.h"
#include <glm/glm.hpp>

namespace Nothofagus
{

/* We will use 32 bits data, so floats and integers have 4 bytes.
 * 1 byte = 8 bits
 */
constexpr unsigned int SIZE_IN_BYTES = 4;

/**
 * @struct DMesh
 * @brief Represents a 2D mesh with OpenGL buffers for rendering.
 * 
 * This struct is used for managing 2D mesh data and OpenGL buffers (VAO, VBO, EBO). It provides methods for initializing the buffers,
 * filling the buffers with mesh data, rendering the mesh, and freeing the GPU memory.
 */
struct DMesh
{
    /// OpenGL buffer identifiers 
    unsigned int vao, vbo, ebo, texture;

    /// The size of the mesh (number of indices)
    std::size_t size;

    /// Transformation matrix for the mesh
    glm::mat3 transform;

    /**
     * @brief Initializes the OpenGL buffers (VAO, VBO, EBO) for the mesh.
     */
    void initBuffers();

    /**
     * @brief Fills the OpenGL buffers with the mesh data (vertices and indices).
     * 
     * @param mesh The mesh data to load into the buffers.
     * @param usage The OpenGL usage flag for buffer data (e.g., GL_STATIC_DRAW).
     */

    void fillBuffers(const Mesh& mesh, unsigned int usage);

    /**
     * @brief Executes the OpenGL draw call to render the mesh.
     * 
     * This method binds the appropriate VAO and executes the `glDrawElements` call to render the mesh.
     */
    void drawCall() const;

    /**
     * @brief Frees the GPU memory used by the mesh buffers.
     * 
     * This method deletes the VAO, VBO, and EBO from the GPU.
     */
    void clear() const;
};

/**
 * @brief Overloaded stream operator for DMesh.
 * 
 * Prints the details of the DMesh object, including the VAO, VBO, EBO, and texture identifiers.
 * 
 * @param os The output stream.
 * @param dMesh The DMesh object.
 * @return The output stream.
 */
std::ostream& operator<<(std::ostream& os, const DMesh& dMesh);

}