
#include "mesh.h"

namespace Nothofagus
{

Mesh& Mesh::operator<<(const Mesh& mesh)
{
    Index offset = indices.size();

    // TODO: resize before pushing new elements

    for (auto const& vertex : mesh.vertices)
        mesh.vertices.push_back(vertex);
    
    for (auto const& index : mesh.indices)
        mesh.indices.push_back(offset + index);

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
            auto& value = *valueIt;
            os << value;
            valueIt++;

            if (valueIt != values.end())
                os << ", ";
        }
        
        os << "]";

        return os;
    }
}

}

std::ostream& operator<<(std::ostream& os, const Vertices& vertices)
{
    return to_ostream(os, vertices);
}

std::ostream& operator<<(std::ostream& os, const Indices& indices)
{
    return to_ostream(os, indices);
}

std::ostream& operator<<(std::ostream& os, const Mesh& mesh)
{
    os << "{ vertices: " << mesh.vertices
       << ", indices: " << mesh.indices
       << "}";

    return os;
}