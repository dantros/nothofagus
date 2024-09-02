#pragma once

#include "text.h"
#include "check.h"
#include <font8x8.h>
#include <iostream>
#include <array>

namespace Nothofagus
{

void writeChar(Texture& texture, std::uint8_t character, std::size_t i0, std::size_t j0, FontType fontType)
{
    debugCheck(i0 + 8 <= texture.size().x and j0 + 8 <= texture.size().y, "Character does not fit inside the texture.");

    std::uint8_t size = 0;
    char* fontPtr = nullptr;

    switch (fontType)
    {
    case FontType::Basic:
        size = 128;
        debugCheck(character < size, "Invalid character for the given font family");
        fontPtr = font8x8_basic[character];
        break;
    case FontType::Control:
        size = 32;
        debugCheck(character < size, "Invalid character for the given font family");
        fontPtr = font8x8_control[character];
        break;
    case FontType::ExtLatin:
        size = 96;
        debugCheck(character < size, "Invalid character for the given font family");
        fontPtr = font8x8_ext_latin[character];
        break;
    case FontType::Greek:
        size = 58;
        debugCheck(character < size, "Invalid character for the given font family");
        fontPtr = font8x8_greek[character];
        break;
    case FontType::Misc:
        size = 10;
        debugCheck(character < size, "Invalid character for the given font family");
        fontPtr = font8x8_misc[character];
        break;
    case FontType::Box:
        size = 128;
        debugCheck(character < size, "Invalid character for the given font family");
        fontPtr = font8x8_box[character];
        break;
    case FontType::Block:
        size = 32;
        debugCheck(character < size, "Invalid character for the given font family");
        fontPtr = font8x8_block[character];
        break;
    case FontType::Hiragana:
        size = 96;
        debugCheck(character < size, "Invalid character for the given font family");
        fontPtr = font8x8_hiragana[character];
        break;
    case FontType::Sga:
        size = 26;
        debugCheck(character < size, "Invalid character for the given font family");
        fontPtr = font8x8_sga[character];
        break;
    default:
        throw;
        break;
    };

    for (std::size_t j = 0; j < 8; ++j)
    {
        std::uint8_t row = fontPtr[j];
        for (std::size_t i = 0; i < 8; ++i)
        {
            const bool fillPixel = ((row >> i) & 1) != 0;
            texture.setPixel(i0 + i, j0 + j, { fillPixel });
        }
        std::cout << std::endl;
    }
}

void writeText(Texture& texture, std::string text, std::size_t i0, std::size_t j0, FontType fontType)
{
    for (std::size_t t = 0; t < text.size(); ++t)
    {
        std::uint8_t character = text.at(t);
        writeChar(texture, character, i0 + t * 8, j0, fontType);
    }
}

}