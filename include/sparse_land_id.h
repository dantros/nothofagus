#pragma once

#include <cstddef>

namespace Nothofagus
{

struct SparsemapId
{
    std::size_t id;

    bool operator==(const SparsemapId& rhs) const
    {
        return id == rhs.id;
    }
};

}
