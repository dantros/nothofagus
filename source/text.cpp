
#include "text.h"
#include "check.h"
#include <font8x8.h>
#include <algorithm>
#include <string>
#include <vector>

namespace Nothofagus
{

namespace
{

/// Returns a pointer to the 8-byte bitmap of `character` in the given font page,
/// or nullptr if the character is out of range for that page.
const unsigned char* glyphBitmap(FontType fontType, std::uint8_t character)
{
    switch (fontType)
    {
    case FontType::Basic:    return character < 128 ? font8x8_basic[character]     : nullptr;
    case FontType::Control:  return character < 32  ? font8x8_control[character]   : nullptr;
    case FontType::ExtLatin: return character < 96  ? font8x8_ext_latin[character] : nullptr;
    case FontType::Greek:    return character < 58  ? font8x8_greek[character]     : nullptr;
    case FontType::Misc:     return character < 10  ? font8x8_misc[character]      : nullptr;
    case FontType::Box:      return character < 128 ? font8x8_box[character]       : nullptr;
    case FontType::Block:    return character < 32  ? font8x8_block[character]     : nullptr;
    case FontType::Hiragana: return character < 96  ? font8x8_hiragana[character]  : nullptr;
    case FontType::Sga:      return character < 26  ? font8x8_sga[character]       : nullptr;
    default:                 return nullptr;
    }
}

/// Paints a glyph into `texture` at pixel offset (i0, j0) of the given `layer`.
/// Every pixel of the 8x8 cell is written (ink = index 1, background = index 0),
/// so the destination layer is fully defined afterwards.
void blitGlyph(IndirectTexture& texture, std::uint8_t character, std::size_t i0, std::size_t j0,
               FontType fontType, std::size_t layer)
{
    const unsigned char* fontPtr = glyphBitmap(fontType, character);
    debugCheck(fontPtr != nullptr, "Invalid character for the given font family");
    if (fontPtr == nullptr)
        return;

    for (std::size_t j = 0; j < 8; ++j)
    {
        std::uint8_t row = fontPtr[j];
        for (std::size_t i = 0; i < 8; ++i)
        {
            const bool fillPixel = ((row >> i) & 1) != 0;
            texture.setPixel(i0 + i, j0 + j, Pixel{static_cast<Pixel::ColorId>(fillPixel ? 1 : 0)}, layer);
        }
    }
}

/// Splits `text` into lines on '\n', dropping any '\r'.
std::vector<std::string> splitLines(const std::string& text)
{
    std::vector<std::string> lines;
    std::string current;
    for (char c : text)
    {
        if (c == '\n')
        {
            lines.push_back(current);
            current.clear();
        }
        else if (c != '\r')
        {
            current.push_back(c);
        }
    }
    lines.push_back(current);
    return lines;
}

/// Tile-map grid dimensions for the given lines: cols = longest line, rows = line
/// count, both clamped to a minimum of 1.
void computeGrid(const std::vector<std::string>& lines, int& cols, int& rows)
{
    rows = std::max<int>(1, static_cast<int>(lines.size()));
    std::size_t maxLength = 1;
    for (const std::string& line : lines)
        maxLength = std::max(maxLength, line.size());
    cols = static_cast<int>(maxLength);
}

/// Builds the row-major cell grid. In the tile-map sampling + screenshot path, cell
/// row 0 lands at the top of the image, so text line `i` maps directly to row `i`.
/// Out-of-page or padding cells reference layer 0 (a blank glyph in the bitmap fonts).
std::vector<std::uint8_t> buildCells(const std::vector<std::string>& lines, int cols, int rows,
                                     std::size_t pageSize)
{
    std::vector<std::uint8_t> cells(static_cast<std::size_t>(cols) * rows, 0);
    for (int i = 0; i < rows; ++i)
    {
        const std::string& line = lines[i];
        for (int c = 0; c < cols; ++c)
        {
            std::uint8_t value = 0;
            if (c < static_cast<int>(line.size()))
            {
                const std::uint8_t code = static_cast<std::uint8_t>(line[c]);
                value = (code < pageSize) ? code : 0;
            }
            cells[static_cast<std::size_t>(i) * cols + c] = value;
        }
    }
    return cells;
}

} // namespace

std::size_t fontPageSize(FontType fontType)
{
    switch (fontType)
    {
    case FontType::Basic:    return 128;
    case FontType::Control:  return 32;
    case FontType::ExtLatin: return 96;
    case FontType::Greek:    return 58;
    case FontType::Misc:     return 10;
    case FontType::Box:      return 128;
    case FontType::Block:    return 32;
    case FontType::Hiragana: return 96;
    case FontType::Sga:      return 26;
    default:                 return 0;
    }
}

void writeChar(IndirectTexture& texture, std::uint8_t character, std::size_t i0, std::size_t j0, FontType fontType)
{
    debugCheck(i0 + 8 <= texture.size().x and j0 + 8 <= texture.size().y, "Character does not fit inside the texture.");
    blitGlyph(texture, character, i0, j0, fontType, 0);
}

IndirectTexture makeTextTexture(std::string text, FontType fontType, glm::vec4 fgColor, glm::vec4 bgColor)
{
    const std::size_t pageSize = fontPageSize(fontType);
    debugCheck(pageSize > 0, "Invalid font type");

    const std::vector<std::string> lines = splitLines(text);
    int cols = 1, rows = 1;
    computeGrid(lines, cols, rows);

    // 8x8 tile, one layer per glyph in the font page.
    IndirectTexture texture({8, 8}, bgColor, pageSize);
    texture.setPallete(ColorPallete{bgColor, fgColor});

    for (std::size_t glyph = 0; glyph < pageSize; ++glyph)
        blitGlyph(texture, static_cast<std::uint8_t>(glyph), 0, 0, fontType, glyph);

    texture.setMap({cols, rows});
    texture.setMapBulk(buildCells(lines, cols, rows, pageSize));

    return texture;
}

void setText(IndirectTexture& texture, std::string text, FontType fontType)
{
    const std::size_t pageSize = fontPageSize(fontType);
    debugCheck(pageSize > 0, "Invalid font type");

    const std::vector<std::string> lines = splitLines(text);
    int cols = 1, rows = 1;
    computeGrid(lines, cols, rows);

    if (texture.mapSize().x != cols or texture.mapSize().y != rows)
        texture.setMap({cols, rows});

    texture.setMapBulk(buildCells(lines, cols, rows, pageSize));
}

}
