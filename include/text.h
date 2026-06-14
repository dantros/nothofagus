#pragma once

#include "texture.h"
#include <string>
#include <cstdint>
#include <cstddef>
#include <glm/glm.hpp>

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

/// Number of glyphs available in a font page.
std::size_t fontPageSize(FontType fontType);

/// Paints a single glyph into an existing IndirectTexture at pixel offset (i0, j0),
/// layer 0. Palette index 1 is the glyph ink, index 0 the background. This is the
/// per-glyph primitive: use it for independently transformable character sprites
/// (e.g. animated text) — for whole strings prefer `makeTextTexture`.
void writeChar(IndirectTexture& texture, std::uint8_t a, std::size_t i0 = 0, std::size_t j0 = 0, FontType fontType = FontType::Basic);

/// Builds a tile-map text texture: an 8x8 tile whose layers are the font glyphs and
/// whose cell grid spells out `text`. The whole string renders as a single bellota /
/// a single draw call; `'\n'` splits the text into tile-map rows (multi-line). Palette
/// index 0 is `bgColor`, index 1 is `fgColor`. Pass the result to `Canvas::addTexture`.
IndirectTexture makeTextTexture(std::string text, FontType fontType = FontType::Basic,
                                glm::vec4 fgColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
                                glm::vec4 bgColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));

/// Re-spells a text texture (built by `makeTextTexture`) in place. Only the cell grid
/// is rewritten — a cheap map-only GPU re-upload; the glyph atlas and palette are left
/// untouched. `fontType` must match the atlas the texture was created with. When the
/// line/column count changes the texture's world size changes too, so a bellota already
/// displaying it must be re-added (its auto-quad is sized at addBellota time).
void setText(IndirectTexture& texture, std::string text, FontType fontType = FontType::Basic);

}
