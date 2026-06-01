#pragma once

#include "texture.h"
#include <variant>

namespace Nothofagus
{

enum class TextureMode
{
    Direct,    ///< RGBA texture (DirectTexture, or render-target proxy color attachment).
    Indirect,  ///< Palette-based IndirectTexture without a cell grid — atlas + palette.
    TileMap,   ///< Palette-based IndirectTexture with a cell grid — atlas + map + palette.
};

/// How `uploadTexture` lays the texture out on the GPU.
enum class TextureUploadMode
{
    Array,  ///< Sprite-pipeline view: GL_TEXTURE_2D_ARRAY / 2D-array (Direct = RGBA, Indirect = R8UI + palette).
    Flat,   ///< ImGui-bindable view: a plain 2D RGBA texture (palette resolved via generateTextureData, layer 0).
};

inline TextureMode textureModeOf(const Texture& texture)
{
    if (std::holds_alternative<IndirectTexture>(texture))
        return std::get<IndirectTexture>(texture).hasMap()
             ? TextureMode::TileMap
             : TextureMode::Indirect;
    return TextureMode::Direct;
}

}
