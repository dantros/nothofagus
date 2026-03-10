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
    glm::vec4 mClearColor{0.0f, 0.0f, 0.0f, 0.0f}; ///< RGBA color used to clear the render target each frame.
};

}
