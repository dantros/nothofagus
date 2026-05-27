
#include "mesh.h"
#include <ostream>

namespace Nothofagus
{

Mesh join(const Mesh& rhs, const Mesh& lhs)
{
    Mesh mesh(rhs);
    const Index offset = static_cast<Index>(mesh.vertices.size());

    mesh.vertices.reserve(mesh.vertices.size() + lhs.vertices.size());
    for (const auto& vertex : lhs.vertices)
        mesh.vertices.push_back(vertex);

    mesh.indices.reserve(mesh.indices.size() + lhs.indices.size());
    for (const auto& index : lhs.indices)
        mesh.indices.push_back(offset + index);

    return mesh;
}

namespace
{
    template <typename ValueT>
    std::ostream& to_ostream(std::ostream& os, const std::vector<ValueT>& values)
    {
        os << "[";

        auto valueIt = values.begin();
        while (valueIt != values.end())
        {
            os << *valueIt;
            ++valueIt;

            if (valueIt != values.end())
                os << ", ";
        }

        os << "]";
        return os;
    }
}

}

std::ostream& operator<<(std::ostream& os, const Nothofagus::Vertex& vertex)
{
    return os << "(" << vertex.position.x << ", " << vertex.position.y
              << ", " << vertex.uv.x << ", " << vertex.uv.y << ")";
}

std::ostream& operator<<(std::ostream& os, const Nothofagus::Vertices& vertices)
{
    return Nothofagus::to_ostream(os, vertices);
}

std::ostream& operator<<(std::ostream& os, const Nothofagus::Indices& indices)
{
    return Nothofagus::to_ostream(os, indices);
}

std::ostream& operator<<(std::ostream& os, const Nothofagus::Mesh& mesh)
{
    os << "{ vertices: " << mesh.vertices
       << ", indices: " << mesh.indices
       << "}";
    return os;
}
