#pragma once

#include <cstddef>

namespace Nothofagus
{

struct TilemapExplorerId
{
    std::size_t id;

    bool operator==(const TilemapExplorerId& rhs) const
    {
        return id == rhs.id;
    }
};

}
