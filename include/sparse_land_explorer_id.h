#pragma once

#include <cstddef>

namespace Nothofagus
{

struct SparseLandExplorerId
{
    std::size_t id;

    bool operator==(const SparseLandExplorerId& rhs) const
    {
        return id == rhs.id;
    }
};

}
