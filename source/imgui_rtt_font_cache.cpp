#include "imgui_rtt_font_cache.h"
#include "check.h"
#include <imgui.h>

namespace Nothofagus
{

ImguiRttFontCache::ImguiRttFontCache(const void* fontData, std::size_t fontDataLen) noexcept
    : mFontData(fontData), mFontDataLen(fontDataLen)
{}

ImFont* ImguiRttFontCache::find(float sizePx) const noexcept
{
    auto it = mCache.find(sizePx);
    return it != mCache.end() ? it->second : nullptr;
}

ImFont& ImguiRttFontCache::at(float sizePx) const
{
    ImFont* font = find(sizePx);
    debugCheck(font != nullptr, "ImguiRttFontCache::at: no font baked at this size — call bake() first or use find()");
    return *font;
}

ImFont& ImguiRttFontCache::bake(float sizePx)
{
    // Dedup: ImGui's AddFontFromMemoryTTF appends a new ImFontConfig and
    // re-rasterises the same glyphs every time, so caching by size avoids
    // bloating the atlas texture on repeat calls.
    if (ImFont* cached = find(sizePx)) return *cached;

    // FontDataOwnedByAtlas = false because the bound TTF buffer is owned by
    // the caller (typically static binary data) and is shared across all
    // font configs created via this cache. Without this ImGui would IM_FREE
    // the same pointer multiple times on shutdown.
    ImFontConfig fontConfig;
    fontConfig.FontDataOwnedByAtlas = false;
    ImFont* font = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(
        const_cast<void*>(mFontData),
        static_cast<int>(mFontDataLen),
        sizePx,
        &fontConfig
    );
    debugCheck(font != nullptr, "AddFontFromMemoryTTF returned null — bound TTF buffer is invalid");
    mCache.emplace(sizePx, font);
    return *font;
}

ImFont& ImguiRttFontCache::setDefaultSize(float sizePx)
{
    ImFont& font = bake(sizePx);
    mDefaultFont = &font;
    return font;
}

}
