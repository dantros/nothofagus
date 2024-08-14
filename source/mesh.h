#pragma once

#include <vector>
#include <string>
#include <iostream>
#include "dvertex.h"

namespace Nothofagus
{

using Coord = float;
using Index = unsigned int;

using Vertices = std::vector<Coord>;
using Indices = std::vector<Index>;

struct Mesh
{
    Vertices vertices;
    Indices indices;
    DVertex dvertex;

    Mesh& operator<<(const Mesh& mesh);
};

Mesh join(const Mesh& rhs, const Mesh& lhs);

}

std::ostream& operator<<(std::ostream& os, const Nothofagus::Vertices& vertices);

std::ostream& operator<<(std::ostream& os, const Nothofagus::Indices& indices);

std::ostream& operator<<(std::ostream& os, const Nothofagus::Mesh& shape);
