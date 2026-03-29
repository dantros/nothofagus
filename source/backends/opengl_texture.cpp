#include "opengl_texture.h"

namespace Nothofagus
{

void OpenGLTexture::clear() const
{
    glDeleteTextures(1, &texture);
}

}
