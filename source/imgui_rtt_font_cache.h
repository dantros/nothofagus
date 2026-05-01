#pragma once

#include <unordered_map>
#include <vector>
#include <cstddef>

struct ImFont;

namespace Nothofagus
{

/// Owns ImFont* handles for fonts the RTT flow uses: the default font baked
/// into newly-created secondary contexts, plus a size→ImFont cache that
/// dedupes Canvas::bakeImguiFont() calls. ImFonts are owned by the shared
/// ImFontAtlas; this class only stores non-owning observers.
class ImguiRttFontCache
{
public:
    /// Bind the TTF buffer at construction. The buffer must outlive the cache —
    /// typically a static byte array embedded in the binary. There is no
    /// rebinding API: the buffer is a class invariant, so bake()/at()/find()
    /// are always callable after construction.
    ImguiRttFontCache(const void* fontData, std::size_t fontDataLen) noexcept;

    /// Look up a previously baked font without ever adding a new one.
    /// Returns nullptr if no font has been baked at this size yet.
    /// Useful when the caller wants to branch on cache miss instead of
    /// paying for a bake.
    ImFont* find(float sizePx) const noexcept;

    /// Look up a previously baked font, asserting it exists. Use when the
    /// caller knows the font has been baked and wants reference semantics
    /// (mirrors std::map::at vs std::map::find).
    ImFont& at(float sizePx) const;

    /// Bake (or return cached) a font at the given pixel size. Repeat calls
    /// with the same sizePx return the same ImFont. FontDataOwnedByAtlas is
    /// always false because the registered TTF buffer is shared across all
    /// font configs created via this cache.
    ImFont& bake(float sizePx);

    /// Bake (if needed) a font at sizePx and register it as the default
    /// (io.FontDefault) for newly-created secondary RTT contexts. Idempotent —
    /// last call wins. Returns the same reference bake() would.
    ImFont& setDefaultSize(float sizePx);

    /// The font registered via setDefaultSize, or nullptr if none. Used by
    /// ImguiRttManager when lazy-creating secondary contexts.
    ImFont* defaultFont() const noexcept { return mDefaultFont; }

    /// Drop the cached ImFont for sizePx. Asserts the size was baked. Clears
    /// mDefaultFont if it pointed at the removed entry. Does NOT touch the
    /// shared ImFontAtlas — caller must orchestrate the atlas Clear + rebuild
    /// + GPU font texture re-upload (typically Canvas::CanvasImpl::drainPendingFontOps).
    void remove(float sizePx);

    /// Drop every cached ImFont* and clear mDefaultFont. The bound TTF buffer
    /// is preserved. Called after ImFontAtlas::Clear() to drop dangling
    /// pointers before re-baking survivors.
    void invalidate() noexcept;

    /// Snapshot of every size currently in the cache, in arbitrary order.
    /// Used during atlas rebuild to know which sizes to re-bake.
    std::vector<float> bakedSizes() const;

private:
    const void* const mFontData;
    const std::size_t mFontDataLen;
    std::unordered_map<float, ImFont*> mCache;
    ImFont* mDefaultFont{nullptr};
};

}
