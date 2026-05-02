#include "imgui_font_manager.h"
#include "check.h"
#include <imgui.h>
#include <cstring>
#include <functional>

namespace Nothofagus
{

namespace
{

// Translate Nothofagus::GlyphRange into the corresponding ImGui glyph-range
// pointer. Default returns nullptr, which makes ImGui fall back to its
// built-in basic Latin + Latin Supplement set.
const ImWchar* glyphRangesFor(GlyphRange range)
{
    ImFontAtlas& atlas = *ImGui::GetIO().Fonts;
    switch (range)
    {
    case GlyphRange::Default:                  return nullptr;
    case GlyphRange::Greek:                    return atlas.GetGlyphRangesGreek();
    case GlyphRange::Cyrillic:                 return atlas.GetGlyphRangesCyrillic();
    case GlyphRange::Korean:                   return atlas.GetGlyphRangesKorean();
    case GlyphRange::Japanese:                 return atlas.GetGlyphRangesJapanese();
    case GlyphRange::ChineseFull:              return atlas.GetGlyphRangesChineseFull();
    case GlyphRange::ChineseSimplifiedCommon:  return atlas.GetGlyphRangesChineseSimplifiedCommon();
    case GlyphRange::Thai:                     return atlas.GetGlyphRangesThai();
    case GlyphRange::Vietnamese:               return atlas.GetGlyphRangesVietnamese();
    }
    return nullptr;
}

/// Adds the main HiDPI ImGui font directly to the atlas (not tracked in the
/// IndexedContainer). The size recipe - `imguiFontSize * contentScale *
/// contentScale` - bakes glyphs at the framebuffer resolution so they remain
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

std::size_t ImguiFontManager::DedupHash::operator()(const DedupKey& key) const noexcept
{
    const std::size_t h1 = std::hash<std::size_t>{}(key.first);
    const std::size_t h2 = std::hash<float>{}(key.second);
    return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
}

ImguiFontManager::ImguiFontManager(const void* fontData,
                                    std::size_t fontDataLen,
                                    float       imguiFontSize) noexcept
    : mFontData(fontData), mFontDataLen(fontDataLen), mImguiFontSize(imguiFontSize)
{}

ImFont* ImguiFontManager::bakeOne(const FontSource& source, float sizePx) const
{
    // FontDataOwnedByAtlas = false: the source's buffer is owned either by
    // the embedded binary (default source) or by FontSource::ttfData (user
    // sources), and is shared across every entry baked from this source plus
    // every atlas rebuild. Without this, ImGui would IM_FREE the same pointer
    // on Clear / shutdown.
    ImFontConfig fontConfig;
    fontConfig.FontDataOwnedByAtlas = false;
    fontConfig.GlyphRanges = glyphRangesFor(source.glyphRange);
    ImFont* font = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(
        const_cast<void*>(source.dataPtr()),
        static_cast<int>(source.dataLen()),
        sizePx,
        &fontConfig
    );
    debugCheck(font != nullptr, "AddFontFromMemoryTTF returned null - source TTF buffer is invalid");
    return font;
}

void ImguiFontManager::initialize(float contentScale)
{
    // Register the bound TTF as the default font source. We use externalData
    // / externalLen so the static binary blob is not memcpy'd into a vector.
    FontSource defaultSource;
    defaultSource.glyphRange  = GlyphRange::Default;
    defaultSource.externalData = mFontData;
    defaultSource.externalLen  = mFontDataLen;
    mDefaultSourceId.id = mSources.add(defaultSource);

    addMainHiDpiFont(mFontData, mFontDataLen, mImguiFontSize, contentScale);
    setDefaultSize(mImguiFontSize);
}

bool ImguiFontManager::hasPendingOps() const noexcept
{
    return !mPendingFontOps.empty();
}

void ImguiFontManager::drainPendingOpsAndRebuildAtlas(float contentScale)
{
    if (mPendingFontOps.empty()) return;

    // 1a. Source removals: cascade-drop every entry attributed to each
    //     victim, then drop the source itself. Assert non-default + registered.
    for (const auto& op : mPendingFontOps)
    {
        if (op.kind != PendingFontOp::Kind::RemoveSource) continue;
        debugCheck(op.sourceId.id != mDefaultSourceId.id,
            "ImguiFontManager: cannot remove the default font source");
        debugCheck(mSources.contains(op.sourceId.id),
            "ImguiFontManager::removeSource: unknown source id");
        dropEntriesForSource(op.sourceId);
        mSources.remove(op.sourceId.id);
    }

    // 1b. Per-id removals. Bake ops are informational - the entry already
    //     exists in mFonts with currentImFont = nullptr; rebakeAll() below
    //     fills it in.
    for (const auto& op : mPendingFontOps)
        if (op.kind == PendingFontOp::Kind::Remove)
            dropEntry(op.id);
    mPendingFontOps.clear();

    // 2. Wipe atlas - every existing ImFont* is dangling.
    ImGui::GetIO().Fonts->Clear();

    // 3. Re-add the main HiDPI font using the same recipe as initialize().
    addMainHiDpiFont(mFontData, mFontDataLen, mImguiFontSize, contentScale);

    // 4. Re-bake every surviving entry from its attributed source; ids and
    //    entry slots stay put, only each entry's currentImFont is patched
    //    to the freshly-rasterised pointer.
    rebakeAll();
}

void ImguiFontManager::remove(ImguiFontId id)
{
    mPendingFontOps.push_back({PendingFontOp::Kind::Remove, id, ImguiFontSourceId{}});
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

void ImguiFontManager::rebakeAll()
{
    for (auto& [rawId, entry] : mFonts)
        entry.currentImFont = bakeOne(mSources.at(entry.sourceId.id), entry.sizePx);
}

ImFont* ImguiFontManager::defaultFont() const noexcept
{
    if (!mDefaultFontId.has_value()) return nullptr;
    return get(*mDefaultFontId);
}

ImguiFontSourceId ImguiFontManager::addSource(std::span<const std::byte> ttfBytes,
                                              GlyphRange glyphRange)
{
    FontSource source;
    source.ttfData.assign(ttfBytes.begin(), ttfBytes.end());
    source.glyphRange = glyphRange;
    return ImguiFontSourceId{ mSources.add(source) };
}

void ImguiFontManager::removeSource(ImguiFontSourceId sourceId)
{
    mPendingFontOps.push_back({PendingFontOp::Kind::RemoveSource, ImguiFontId{}, sourceId});
}

bool ImguiFontManager::containsSource(ImguiFontSourceId sourceId) const noexcept
{
    return mSources.contains(sourceId.id);
}

ImguiFontId ImguiFontManager::bake(ImguiFontSourceId sourceId, float sizePx)
{
    const ImguiFontId id = getOrCreate(sourceId, sizePx);
    // If the entry was created with currentImFont = nullptr (atlas locked),
    // schedule a rebuild so the next drain rebakes it.
    if (get(id) == nullptr)
        mPendingFontOps.push_back({PendingFontOp::Kind::Bake, id, ImguiFontSourceId{}});
    return id;
}

ImguiFontId ImguiFontManager::getOrCreate(ImguiFontSourceId sourceId, float sizePx)
{
    debugCheck(mSources.contains(sourceId.id),
        "ImguiFontManager::getOrCreate: unknown source id");

    const DedupKey key{sourceId.id, sizePx};
    if (auto it = mDedup.find(key); it != mDedup.end())
        return it->second;

    // Sync bake when the atlas is unlocked; defer otherwise (entry sits with
    // currentImFont = nullptr until rebakeAll() runs).
    ImFont* font = ImGui::GetIO().Fonts->Locked
        ? nullptr
        : bakeOne(mSources.at(sourceId.id), sizePx);

    const std::size_t rawId = mFonts.add({sizePx, sourceId, font});
    const ImguiFontId id{rawId};
    mDedup.emplace(key, id);
    return id;
}

ImguiFontId ImguiFontManager::setDefaultSize(float sizePx)
{
    const ImguiFontId id = getOrCreate(mDefaultSourceId, sizePx);
    mDefaultFontId = id;
    return id;
}

void ImguiFontManager::dropEntry(ImguiFontId id)
{
    debugCheck(mFonts.contains(id.id), "ImguiFontManager::dropEntry: unknown id");
    const FontEntry& entry = mFonts.at(id.id);
    if (mDefaultFontId.has_value() && *mDefaultFontId == id)
        mDefaultFontId.reset();
    mDedup.erase(DedupKey{entry.sourceId.id, entry.sizePx});
    mFonts.remove(id.id);
}

void ImguiFontManager::dropEntriesForSource(ImguiFontSourceId sourceId)
{
    // Two-pass: collect ids first so we don't mutate mFonts while iterating it.
    std::vector<ImguiFontId> toDrop;
    for (const auto& [rawId, entry] : mFonts)
        if (entry.sourceId == sourceId)
            toDrop.push_back(ImguiFontId{rawId});
    for (const ImguiFontId id : toDrop)
        dropEntry(id);
}

}
