#pragma once

#include <glad/glad.h>

namespace Nothofagus
{

struct OpenGLTexture
{
    GLuint texture;
    bool   isFlat = false;  ///< true for a TextureUploadMode::Flat GL_TEXTURE_2D (ImGui-bindable).

    void clear() const;
};

}
