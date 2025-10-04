#pragma once

#include "texture.h"
#include <string>
#include <cstdint>

namespace Nothofagus
{

enum class FontType: std::uint8_t {
    Basic = 0,
    Control,
    ExtLatin,
    Greek,
    Misc,
    Box,
    Block,
    Hiragana,
    Sga,
    Count
};

void writeChar(IndirectTexture& texture, std::uint8_t a, std::size_t i0 = 0, std::size_t j0 = 0, FontType fontType = FontType::Basic);
void writeText(IndirectTexture& texture, std::string text, std::size_t i0 = 0, std::size_t j0 = 0, FontType fontType = FontType::Basic);

}