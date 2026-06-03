#pragma once

#include <cstddef>

namespace Nothofagus
{

struct DenseLandId
{
    std::size_t id;

    bool operator==(const DenseLandId& rhs) const
    {
        return id == rhs.id;
    }
};

}
