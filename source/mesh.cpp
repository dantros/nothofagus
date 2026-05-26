
#include "mesh.h"
#include <ostream>

namespace Nothofagus
{

Mesh& Mesh::operator<<(const Mesh& other)
{
    const Index offset = static_cast<Index>(vertices.size());

    vertices.reserve(vertices.size() + other.vertices.size());
    for (const auto& vertex : other.vertices)
        vertices.push_back(vertex);

    indices.reserve(indices.size() + other.indices.size());
    for (const auto& index : other.indices)
        indices.push_back(offset + index);

    return *this;
}

Mesh join(const Mesh& rhs, const Mesh& lhs)
{
    Mesh mesh(rhs);
    mesh << lhs;
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
    return os << "(" << vertex.x << ", " << vertex.y << ", " << vertex.u << ", " << vertex.v << ")";
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
