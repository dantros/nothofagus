#pragma once

#include <cstddef>
#include <glm/glm.hpp>
#include "bellota.h"

namespace Nothofagus
{

struct RenderTargetId
{
    std::size_t id;

    bool operator==(const RenderTargetId& rhs) const
    {
        return id == rhs.id;
    }
};

struct RenderTarget
{
    glm::ivec2 mSize;
    TextureId mProxyTextureId; ///< The TextureId bellotas use to sample this render target.
};

}
