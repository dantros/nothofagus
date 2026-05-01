#include "imgui_font_manager.h"
#include "check.h"
#include <imgui.h>

namespace Nothofagus
{

ImguiFontManager::ImguiFontManager(const void* fontData,
                                    std::size_t fontDataLen,
                                    float       imguiFontSize) noexcept
    : mFontData(fontData), mFontDataLen(fontDataLen), mImguiFontSize(imguiFontSize)
{}

ImFont* ImguiFontManager::bakeOne(float sizePx) const
{
    // FontDataOwnedByAtlas = false because the bound TTF buffer is owned by
    // the caller (typically static binary data) and is shared across every
    // entry in this manager and across atlas rebuilds. Without this, ImGui
    // would IM_FREE the same pointer multiple times on Clear / shutdown.
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

namespace
{

/// Adds the main HiDPI ImGui font directly to the atlas (not tracked in the
/// IndexedContainer). The size recipe — `imguiFontSize * contentScale *
/// contentScale` — bakes glyphs at the framebuffer resolution so they remain
/// crisp and OS-DPI-scaled on the main-canvas UI. Same FontDataOwnedByAtlas
/// = false discipline as bakeOne so atlas Clear / shutdown can't double-free.
void addMainHiDpiFont(const void* fontData,
                      std::size_t fontDataLen,
                      float       imguiFontSize,
                      float       contentScale)
{
    ImFontConfig fontConfig;
    fontConfig.FontDataOwnedByAtlas = false;
    ImGui::GetIO().Fonts->AddFontFromMemoryTTF(
        const_cast<void*>(fontData),
        static_cast<int>(fontDataLen),
        imguiFontSize * contentScale * contentScale,
        &fontConfig
    );
}

}

void ImguiFontManager::initialize(float contentScale)
{
    addMainHiDpiFont(mFontData, mFontDataLen, mImguiFontSize, contentScale);
    setDefaultSize(mImguiFontSize);
}

ImguiFontId ImguiFontManager::bake(float sizePx)
{
    const ImguiFontId id = getOrCreate(sizePx);
    // If the entry was created with currentImFont = nullptr (atlas locked),
    // schedule a rebuild so the next drain rebakes it.
    if (get(id) == nullptr)
        mPendingFontOps.push_back({PendingFontOp::Kind::Bake, id});
    return id;
}

void ImguiFontManager::remove(ImguiFontId id)
{
    mPendingFontOps.push_back({PendingFontOp::Kind::Remove, id});
}

bool ImguiFontManager::hasPendingOps() const noexcept
{
    return !mPendingFontOps.empty();
}

void ImguiFontManager::drainPendingOpsAndRebuildAtlas(float contentScale)
{
    if (mPendingFontOps.empty()) return;

    // 1. Apply pending Remove ops to the cache map (no atlas touch yet).
    //    Bake ops are informational — the entry already exists in mFonts
    //    with currentImFont = nullptr; rebakeAll() below fills it in.
    for (const auto& op : mPendingFontOps)
        if (op.kind == PendingFontOp::Kind::Remove)
            dropEntry(op.id);
    mPendingFontOps.clear();

    // 2. Wipe atlas — every existing ImFont* is dangling.
    ImGui::GetIO().Fonts->Clear();

    // 3. Re-add the main HiDPI font using the same recipe as initialize().
    addMainHiDpiFont(mFontData, mFontDataLen, mImguiFontSize, contentScale);

    // 4. Re-bake every surviving entry; ids and entry slots stay put, only
    //    each entry's currentImFont is patched to the freshly-rasterised pointer.
    rebakeAll();
}

ImguiFontId ImguiFontManager::getOrCreate(float sizePx)
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

ImFont* ImguiFontManager::get(ImguiFontId id) const noexcept
{
    if (!mFonts.contains(id.id)) return nullptr;
    return mFonts.at(id.id).currentImFont;
}

bool ImguiFontManager::contains(ImguiFontId id) const noexcept
{
    return mFonts.contains(id.id);
}

void ImguiFontManager::dropEntry(ImguiFontId id)
{
    debugCheck(mFonts.contains(id.id), "ImguiFontManager::dropEntry: unknown id");
    const float sizePx = mFonts.at(id.id).sizePx;
    if (mDefaultFontId.has_value() && *mDefaultFontId == id)
        mDefaultFontId.reset();
    mSizeToId.erase(sizePx);
    mFonts.remove(id.id);
}

void ImguiFontManager::rebakeAll()
{
    for (auto& [rawId, entry] : mFonts)
        entry.currentImFont = bakeOne(entry.sizePx);
}

ImguiFontId ImguiFontManager::setDefaultSize(float sizePx)
{
    const ImguiFontId id = getOrCreate(sizePx);
    mDefaultFontId = id;
    return id;
}

ImFont* ImguiFontManager::defaultFont() const noexcept
{
    if (!mDefaultFontId.has_value()) return nullptr;
    return get(*mDefaultFontId);
}

}
