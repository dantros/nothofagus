#pragma once

namespace Nothofagus
{

/**
 * @brief A structure representing a 2D vertex with texture coordinates.
 * 
 * The `DVertex` structure holds the position (x, y) of the vertex and its associated texture coordinates (tx, ty).
 * It is typically used in rendering pipelines where vertices are defined by their positions in space and their corresponding texture mapping.
 */
struct DVertex
{
    float x, y; ///< The position of the vertex in 2D space (x, y).
    float tx, ty; ///< The texture coordinates of the vertex (tx, ty).

    /**
     * @brief Returns the number of floats required to represent this vertex data.
     * 
     * This function returns the number of float values used to store the vertex's position and texture coordinates.
     * In this case, it always returns 4, since the vertex consists of 4 floats: 2 for position and 2 for texture coordinates.
     * 
     * @return The number of floats representing the vertex (always 4).
     */
    unsigned int arity() const { return 4; }
};

}