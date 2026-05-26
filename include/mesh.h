#pragma once

#include <cstddef>
#include <vector>
#include <iosfwd>

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
    float x;  ///< Position x in canvas pixels (origin centered on the bellota).
    float y;  ///< Position y in canvas pixels.
    float u;  ///< Texture coordinate u in [0, 1].
    float v;  ///< Texture coordinate v in [0, 1].
};

/// Triangle index — vertex offsets are 32-bit unsigned integers.
using Index = unsigned int;

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

    /// Appends another mesh's vertices and (re-offset) indices to this one.
    Mesh& operator<<(const Mesh& other);
};

/// Returns a new mesh built by appending `lhs` after `rhs`.
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

#include <functional>

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
