#pragma once

#include <vector>
#include <string>
#include <iostream>

namespace Nothofagus
{

/**
 * @brief Alias for the coordinate type in 3D space, represented as a floating-point value.
 */
using Coord = float;

/**
 * @brief Alias for the index type used for vertices and elements in a mesh, represented as an unsigned integer.
 */
using Index = unsigned int;

/**
 * @brief A collection of coordinates representing vertices in a 3D mesh.
 * 
 * The `Vertices` type is a vector of coordinates (`Coord`), representing the positions of vertices in space. 
 * Each vertex is represented by a coordinate (x, y), but the exact interpretation of these coordinates depends on the mesh data.
 */
using Vertices = std::vector<Coord>;

/**
 * @brief A collection of indices for vertices in a mesh.
 * 
 * The `Indices` type is a vector of indices (`Index`) that define the connectivity of the vertices. 
 * These indices refer to the positions of the vertices in the `Vertices` array, forming triangles or other primitives in the mesh.
 */
using Indices = std::vector<Index>;


/**
 * @brief A mesh representation consisting of vertices and indices.
 * 
 * The `Mesh` structure represents a mesh, including:
 * - A list of vertices (`vertices`), each represented by a coordinate.
 * - A list of indices (`indices`), where each index refers to a vertex in the `vertices` array to define the shape of the mesh.
 */
struct Mesh
{
    Vertices vertices; ///< The collection of vertex coordinates.
    Indices indices;   ///< The collection of vertex indices.

    /**
     * @brief Adds the vertices and indices of another mesh to this mesh.
     * 
     * This operator appends the vertices and indices of another mesh (`mesh`) to the current mesh (`*this`).
     * The indices of the new mesh are adjusted by adding the size of the current indices to avoid index collisions.
     * 
     * @param mesh The mesh to append.
     * @return A reference to the updated mesh.
     */
    Mesh& operator<<(const Mesh& mesh);
};

/**
 * @brief Joins two meshes into one.
 * 
 * This function joins two meshes (`rhs` and `lhs`) by adding the vertices and indices of `lhs` to `rhs`.
 * 
 * @param rhs The mesh to append to.
 * @param lhs The mesh to be appended.
 * @return A new mesh resulting from the concatenation of `rhs` and `lhs`.
 */
Mesh join(const Mesh& rhs, const Mesh& lhs);

}

/**
 * @brief Streams the `Vertices` array to an output stream.
 * 
 * This function prints a vector of vertices to an output stream in a readable format.
 * 
 * @param os The output stream.
 * @param vertices The vertices array to print.
 * @return The output stream.
 */
std::ostream& operator<<(std::ostream& os, const Nothofagus::Vertices& vertices);

/**
 * @brief Streams the `Indices` array to an output stream.
 * 
 * This function prints a vector of indices to an output stream in a readable format.
 * 
 * @param os The output stream.
 * @param indices The indices array to print.
 * @return The output stream.
 */
std::ostream& operator<<(std::ostream& os, const Nothofagus::Indices& indices);

/**
 * @brief Streams a `Mesh` object to an output stream.
 * 
 * This function prints the mesh, including its vertices and indices, to an output stream in a readable format.
 * 
 * @param os The output stream.
 * @param mesh The mesh to print.
 * @return The output stream.
 */
std::ostream& operator<<(std::ostream& os, const Nothofagus::Mesh& shape);
