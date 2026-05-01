#pragma once

#include "imgui_font_id.h"
#include "indexed_container.h"
#include <unordered_map>
#include <optional>
#include <cstddef>

struct ImFont;

namespace Nothofagus
{

/// Owns ImFont* handles for fonts the RTT flow uses: each font is a stable
/// entry in an IndexedContainer (referenced by ImguiFontId) plus an optional
/// "default" pointer used as io.FontDefault on newly-created secondary RTT
/// contexts. ImFonts themselves are owned by the shared ImFontAtlas; entries
/// here only carry non-owning observers, and rebakeAll() patches them after
/// an atlas Clear without changing ids.
class ImguiRttFontCache
{
public:
    /// Bind the TTF buffer at construction. The buffer must outlive the cache —
    /// typically a static byte array embedded in the binary. There is no
    /// rebinding API: the buffer is a class invariant.
    ImguiRttFontCache(const void* fontData, std::size_t fontDataLen) noexcept;

    /// Get the id for sizePx, creating a new entry if no font has been baked
    /// at this size yet. On creation: if the atlas is unlocked, bakes
    /// immediately and stores the ImFont*; if locked, leaves currentImFont
    /// null — caller is responsible for triggering rebakeAll() at a safe
    /// point (typically Canvas::CanvasImpl::drainPendingFontOps).
    /// Repeat calls with the same sizePx return the same id.
    ImguiFontId getOrCreate(float sizePx);

    /// Resolve an id to its current ImFont*. Returns nullptr if the entry
    /// does not exist (id is unknown or was removed) or if the entry exists
    /// but its bake is still pending (deferred).
    ImFont* get(ImguiFontId id) const noexcept;

    /// True if the id is currently registered in the container.
    bool contains(ImguiFontId id) const noexcept;

    /// Drop the entry for id. Asserts id is valid. Clears mDefaultFontId
    /// if it referred to the removed entry. Does NOT touch the atlas —
    /// caller orchestrates the Clear + rebakeAll + GPU re-upload.
    void remove(ImguiFontId id);

    /// Walk every entry and (re-)bake it from the bound TTF buffer, updating
    /// each entry's currentImFont. Used after ImFontAtlas::Clear() to
    /// repopulate the atlas + repair every cached pointer in one pass.
    /// Ids and entry slots are preserved.
    void rebakeAll();

    /// Bake (if needed) at sizePx and register that id as the secondary-
    /// context default. Idempotent. Returns the id.
    ImguiFontId setDefaultSize(float sizePx);

    /// Current default ImFont*, or nullptr if no default set, the default
    /// id was removed, or the default's bake is still pending.
    ImFont* defaultFont() const noexcept;

private:
    struct FontEntry
    {
        float   sizePx;
        ImFont* currentImFont;  // null while a deferred bake is pending
    };

    /// Calls ImGui::GetIO().Fonts->AddFontFromMemoryTTF with mFontData/Len
    /// and FontDataOwnedByAtlas=false. Asserts non-null.
    ImFont* bakeOne(float sizePx) const;

    const void* const mFontData;
    const std::size_t mFontDataLen;
    IndexedContainer<FontEntry>            mFonts;
    std::unordered_map<float, ImguiFontId> mSizeToId;
    std::optional<ImguiFontId>             mDefaultFontId;
};

}
