#pragma once

#include <cstddef>

namespace Nothofagus
{

struct SparseLandId
{
    std::size_t id;

    bool operator==(const SparseLandId& rhs) const
    {
        return id == rhs.id;
    }
};

}
