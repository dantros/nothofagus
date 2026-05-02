#pragma once

#include <cstddef>

namespace Nothofagus
{

/// Stable handle to a TTF buffer registered via Canvas::addImguiFontSource().
/// One source can be baked at any number of sizes; each bake produces its own
/// ImguiFontId. The source id stays valid until removeImguiFontSource(thisId).
struct ImguiFontSourceId
{
    std::size_t id;

    bool operator==(const ImguiFontSourceId& rhs) const { return id == rhs.id; }
};

/// Subset of ImGui's built-in glyph range presets, exposed without leaking
/// imgui.h. Maps to ImFontAtlas::GetGlyphRangesXxx() inside the implementation.
enum class GlyphRange
{
    Default,                    // basic Latin + Latin Supplement
    Greek,
    Cyrillic,
    Korean,
    Japanese,                   // Hiragana, Katakana, common Kanji
    ChineseFull,
    ChineseSimplifiedCommon,
    Thai,
    Vietnamese,
};

}
