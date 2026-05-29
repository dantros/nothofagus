#pragma once

#include <glm/glm.hpp>
#include <cstddef>
#include <functional>

namespace Nothofagus
{

/// Hash functor for `glm::ivec2` so it can be used as an `unordered_map` / `unordered_set` key.
/// Combines the two int components into a `std::size_t` via a Boost-style hash combine.
struct IVec2Hash
{
    std::size_t operator()(const glm::ivec2& v) const noexcept
    {
        const std::size_t hx = std::hash<int>{}(v.x);
        const std::size_t hy = std::hash<int>{}(v.y);
        return hx ^ (hy + 0x9e3779b97f4a7c15ULL + (hx << 6) + (hx >> 2));
    }
};

}
