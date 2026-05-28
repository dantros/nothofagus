#pragma once

#include <cstddef>

namespace Nothofagus
{

struct TilemapViewId
{
    std::size_t id;

    bool operator==(const TilemapViewId& rhs) const
    {
        return id == rhs.id;
    }
};

}
