#pragma once

#include <glm/glm.hpp>

namespace Nothofagus
{

/**
 * @brief GPU-side framebuffer object for render-to-texture.
 *
 * Owns an FBO, a GL_TEXTURE_2D_ARRAY color attachment (1 layer), and a
 * depth renderbuffer. The color texture handle can be used directly as a
 * DMesh texture so bellotas can sample the rendered result.
 */
struct DFramebuffer
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
