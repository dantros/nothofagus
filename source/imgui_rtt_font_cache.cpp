#include "imgui_rtt_font_cache.h"
#include "check.h"
#include <imgui.h>

namespace Nothofagus
{

ImguiRttFontCache::ImguiRttFontCache(const void* fontData, std::size_t fontDataLen) noexcept
    : mFontData(fontData), mFontDataLen(fontDataLen)
{}

ImFont* ImguiRttFontCache::bakeOne(float sizePx) const
{
    // FontDataOwnedByAtlas = false because the bound TTF buffer is owned by
    // the caller (typically static binary data) and is shared across every
    // entry in this cache. Without this ImGui would IM_FREE the same pointer
    // multiple times on shutdown.
    ImFontConfig fontConfig;
    fontConfig.FontDataOwnedByAtlas = false;
    ImFont* font = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(
        const_cast<void*>(mFontData),
        static_cast<int>(mFontDataLen),
        sizePx,
        &fontConfig
    );
    debugCheck(font != nullptr, "AddFontFromMemoryTTF returned null — bound TTF buffer is invalid");
    return font;
}

ImguiFontId ImguiRttFontCache::getOrCreate(float sizePx)
{
    if (auto it = mSizeToId.find(sizePx); it != mSizeToId.end())
        return it->second;

    // Sync bake when the atlas is unlocked; defer otherwise (entry sits with
    // currentImFont = nullptr until rebakeAll() runs).
    ImFont* font = ImGui::GetIO().Fonts->Locked ? nullptr : bakeOne(sizePx);

    const std::size_t rawId = mFonts.add({sizePx, font});
    const ImguiFontId id{rawId};
    mSizeToId.emplace(sizePx, id);
    return id;
}

ImFont* ImguiRttFontCache::get(ImguiFontId id) const noexcept
{
    if (!mFonts.contains(id.id)) return nullptr;
    return mFonts.at(id.id).currentImFont;
}

bool ImguiRttFontCache::contains(ImguiFontId id) const noexcept
{
    return mFonts.contains(id.id);
}

void ImguiRttFontCache::remove(ImguiFontId id)
{
    debugCheck(mFonts.contains(id.id), "ImguiRttFontCache::remove: unknown id");
    const float sizePx = mFonts.at(id.id).sizePx;
    if (mDefaultFontId.has_value() && *mDefaultFontId == id)
        mDefaultFontId.reset();
    mSizeToId.erase(sizePx);
    mFonts.remove(id.id);
}

void ImguiRttFontCache::rebakeAll()
{
    for (auto& [rawId, entry] : mFonts)
        entry.currentImFont = bakeOne(entry.sizePx);
}

ImguiFontId ImguiRttFontCache::setDefaultSize(float sizePx)
{
    const ImguiFontId id = getOrCreate(sizePx);
    mDefaultFontId = id;
    return id;
}

ImFont* ImguiRttFontCache::defaultFont() const noexcept
{
    if (!mDefaultFontId.has_value()) return nullptr;
    return get(*mDefaultFontId);
}

}
