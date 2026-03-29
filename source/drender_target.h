#pragma once

#include <cstddef>
#include <glm/glm.hpp>

namespace Nothofagus
{

struct DRenderTarget
{
    std::size_t id;
    glm::ivec2 size; ///< Cached size — used by canvas_impl to compute RTT world transform.
};

}
