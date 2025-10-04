#include "dtexture.h"
#include <glad/glad.h>

namespace Nothofagus
{

void DTexture::clear() const
{
    glDeleteTextures(1, &texture);
}

}