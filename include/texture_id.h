#pragma once

#include <cstddef>
#include <functional>

namespace Nothofagus
{

struct TextureId
{
    std::size_t id;

    bool operator==(const TextureId& rhs) const
    {
        return id == rhs.id;
    }
};

}

namespace std
{

template<>
struct hash<Nothofagus::TextureId>
{
    std::size_t operator()(const Nothofagus::TextureId& textureId) const
    {
        return std::hash<std::size_t>{}(textureId.id);
    }
};

}
