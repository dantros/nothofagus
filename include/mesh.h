#pragma once

#include <glm/glm.hpp>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <iosfwd>
#include <functional>

namespace Nothofagus
{

/**
 * @brief A single vertex in a Nothofagus mesh: 2D position plus UV coordinates.
 *
 * The layout is fixed (two floats for position, two floats for UV) and matches
 * the vertex format consumed by the OpenGL and Vulkan rendering pipelines.
 */
struct Vertex
{
    glm::vec2 position;  ///< Position in canvas pixels (origin centered on the bellota).
    glm::vec2 uv;        ///< Texture coordinate in [0, 1].
};

/// Triangle index — vertex offsets are 32-bit unsigned integers.
using Index = std::uint32_t;

/// Vertex list type backing a Mesh.
using Vertices = std::vector<Vertex>;

/// Index list type backing a Mesh.
using Indices = std::vector<Index>;

/**
 * @brief A triangle mesh: a vertex list and a triangle index list.
 *
 * Users build meshes directly in this layout and hand them to
 * `Canvas::addMesh(...)` to obtain a `MeshId` that can be attached to a Bellota.
 */
struct Mesh
{
    Vertices vertices;
    Indices  indices;

    /// Appends `other`'s vertices and (re-offset) indices to this mesh.
    Mesh& operator<<(const Mesh& other);
};

/// Returns a new mesh built by concatenating `rhs` and `lhs`, re-offsetting
/// the appended indices so they point into the merged vertex array.
Mesh join(const Mesh& rhs, const Mesh& lhs);

/**
 * @brief Stable handle to a Mesh registered in a Canvas.
 */
struct MeshId
{
    std::size_t id;

    bool operator==(const MeshId& rhs) const { return id == rhs.id; }
};

}

namespace std
{

template<>
struct hash<Nothofagus::MeshId>
{
    std::size_t operator()(const Nothofagus::MeshId& meshId) const
    {
        return std::hash<std::size_t>{}(meshId.id);
    }
};

}

std::ostream& operator<<(std::ostream& os, const Nothofagus::Vertex& vertex);
std::ostream& operator<<(std::ostream& os, const Nothofagus::Vertices& vertices);
std::ostream& operator<<(std::ostream& os, const Nothofagus::Indices& indices);
std::ostream& operator<<(std::ostream& os, const Nothofagus::Mesh& mesh);
