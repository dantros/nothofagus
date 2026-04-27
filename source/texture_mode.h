#pragma once

#include "texture.h"
#include <variant>

namespace Nothofagus
{

enum class TextureMode
{
    Direct,    ///< RGBA texture (DirectTexture, or render-target proxy color attachment).
    Indirect,  ///< Palette-based IndirectTexture — index texture + palette texture.
    TileMap,   ///< TileMapTexture — atlas texture + map texture.
};

inline TextureMode textureModeOf(const Texture& texture)
{
    if (std::holds_alternative<IndirectTexture>(texture)) return TextureMode::Indirect;
    if (std::holds_alternative<TileMapTexture>(texture))  return TextureMode::TileMap;
    return TextureMode::Direct;
}

}
