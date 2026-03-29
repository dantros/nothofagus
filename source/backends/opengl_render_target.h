#pragma once

#include <glm/glm.hpp>

namespace Nothofagus
{

struct OpenGLRenderTarget
{
    unsigned int fbo;
    unsigned int colorTexture; ///< GL_TEXTURE_2D_ARRAY, 1 layer — shared with the proxy TexturePack.
    unsigned int depthRbo;
    glm::ivec2 size;

    void create(glm::ivec2 size);
    void bind() const;
    void unbind() const;
    void clear() const;
};

}
