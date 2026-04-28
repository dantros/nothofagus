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

inline TextureMode textureModeOf(const Texture& texture)
{
    if (std::holds_alternative<IndirectTexture>(texture))
        return std::get<IndirectTexture>(texture).hasMap()
             ? TextureMode::TileMap
             : TextureMode::Indirect;
    return TextureMode::Direct;
}

}
