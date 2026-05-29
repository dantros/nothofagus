#pragma once

#include <cstddef>

namespace Nothofagus
{

struct SparsemapExplorerId
{
    std::size_t id;

    bool operator==(const SparsemapExplorerId& rhs) const
    {
        return id == rhs.id;
    }
};

}
