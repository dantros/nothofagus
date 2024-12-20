#pragma once

namespace Nothofagus
{

/**
 * @brief A structure representing a 2D texture in the GPU.
 * 
 * The `DTexture` structure holds a handle to a 2D texture that is used in rendering.
 * This texture is typically stored in GPU memory and can be used for various rendering operations such as applying textures to 3D models or 2D sprites.
 */
struct DTexture
{
    unsigned int texture; ///< The OpenGL texture handle for a 2D texture in GPU memory.
};

/**
 * @brief A structure representing a texture array in the GPU.
 * 
 * The `DTextureArray` structure holds a handle to a texture array, which is a collection of textures stored in GPU memory.
 * Texture arrays are often used for multi-texture operations or for accessing different texture layers in a single draw call.
 */
struct DTextureArray
{
    unsigned int textureArray; ///< The OpenGL texture array handle for a collection of textures stored in GPU memory.
};

}