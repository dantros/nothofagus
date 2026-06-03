#pragma once

#include <cstddef>

namespace Nothofagus
{

struct TilemapId
{
    std::size_t id;

    bool operator==(const TilemapId& rhs) const
    {
        return id == rhs.id;
    }
};

}
