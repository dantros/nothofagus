#pragma once

#include <cstddef>

namespace Nothofagus
{

struct HeightmapTerrainId
{
    std::size_t id;

    bool operator==(const HeightmapTerrainId& rhs) const { return id == rhs.id; }
};

} // namespace Nothofagus
