#include "dframebuffer.h"
#include <glad/glad.h>

namespace Nothofagus
{

void DFramebuffer::create(glm::ivec2 size)
{
    this->size = size;

    // Color attachment: GL_TEXTURE_2D_ARRAY with 1 layer to match the existing
    // sampler2DArray shader path (layerIndex = 0).
    glGenTextures(1, &colorTexture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, colorTexture);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, size.x, size.y, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    // Depth renderbuffer so Z-ordering within the RTT pass works correctly.
    glGenRenderbuffers(1, &depthRbo);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size.x, size.y);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    // Framebuffer object.
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, colorTexture, 0, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRbo);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void DFramebuffer::bind() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
}

void DFramebuffer::unbind() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void DFramebuffer::clear() const
{
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &colorTexture);
    glDeleteRenderbuffers(1, &depthRbo);
}

}
