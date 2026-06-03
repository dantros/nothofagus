#pragma once

#include <cstddef>

namespace Nothofagus
{

struct DenseLandExplorerId
{
    std::size_t id;

    bool operator==(const DenseLandExplorerId& rhs) const
    {
        return id == rhs.id;
    }
};

}
