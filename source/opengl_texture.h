#pragma once

#include <glad/glad.h>

namespace Nothofagus
{

struct OpenGLTexture
{
    GLuint texture;

    void clear() const;
};

}
