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

/// Adds the main ImGui UI font directly to the atlas (not tracked in the
/// IndexedContainer). Baked at the logical `imguiFontSize` (points).
///
/// ImGui 1.92's dynamic font atlas (ImGuiBackendFlags_RendererHasTextures, set
/// by every render backend here) rasterizes glyphs at the actual displayed
/// pixel density each frame, so HiDPI crispness is automatic — the bake size is
/// the *layout* size, not a pre-scaled pixel size. Baking at `imguiFontSize`
/// keeps UI text at a fixed fraction of the game canvas, scaling together with
/// the letterboxed sprites instead of by OS DPI, and makes `GetFontSize()`
/// return `imguiFontSize` on every backend / contentScale. The previous
/// `* contentScale * contentScale` recipe over-inflated text on HiDPI displays.
///
/// (`contentScale` is retained in the signature so the plumbing is ready for a
/// future opt-in DPI scale; it no longer multiplies the size.) The regular face
/// is a stb-compressed blob, so it goes through AddFontFromMemoryCompressedTTF,
/// which always decompresses into a fresh atlas-owned buffer - the static
/// compressed source stays available for every atlas rebuild.
void addMainHiDpiFont(const void* fontData,
                      std::size_t fontDataLen,
                      float       imguiFontSize,
                      float       contentScale)
{
    (void)contentScale;
    ImFontConfig fontConfig;
    fontConfig.FontDataOwnedByAtlas = false;
    ImGui::GetIO().Fonts->AddFontFromMemoryCompressedTTF(
        fontData,
        static_cast<int>(fontDataLen),
        imguiFontSize,
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

ImguiFontManager::ImguiFontManager(const EmbeddedFontFamily& family,
                                    float                     imguiFontSize) noexcept
    : mFamily(family), mImguiFontSize(imguiFontSize)
{}

ImFont* ImguiFontManager::bakeOne(const FontSource& source, float sizePx) const
{
    // FontDataOwnedByAtlas = false: the source's buffer is owned either by
    // the embedded binary (built-in faces) or by FontSource::ttfData (user
    // sources), and is shared across every entry baked from this source plus
    // every atlas rebuild. Without this, ImGui would IM_FREE the same pointer
    // on Clear / shutdown.
    //
    // Embedded faces are stb-compressed -> AddFontFromMemoryCompressedTTF
    // (decompresses into a fresh atlas-owned buffer each bake, leaving our
    // static compressed source untouched). User sources are raw TTF.
    ImFontConfig fontConfig;
    fontConfig.FontDataOwnedByAtlas = false;
    fontConfig.GlyphRanges = glyphRangesFor(source.glyphRange);
    ImFont* font = source.compressed
        ? ImGui::GetIO().Fonts->AddFontFromMemoryCompressedTTF(
              source.dataPtr(),
              static_cast<int>(source.dataLen()),
              sizePx,
              &fontConfig)
        : ImGui::GetIO().Fonts->AddFontFromMemoryTTF(
              const_cast<void*>(source.dataPtr()),
              static_cast<int>(source.dataLen()),
              sizePx,
              &fontConfig);
    debugCheck(font != nullptr, "AddFontFromMemory*TTF returned null - source font buffer is invalid");
    return font;
}

void ImguiFontManager::initialize(float contentScale)
{
    // Register each built-in face as a non-owning font source. externalData /
    // externalLen point straight at the embedded binary blobs so nothing is
    // memcpy'd into a vector.
    auto addBuiltin = [this](const EmbeddedFace& face,
                             GlyphRange glyphRange = GlyphRange::Default) -> ImguiFontSourceId
    {
        FontSource source;
        source.glyphRange   = glyphRange;
        source.externalData = face.data;
        source.externalLen  = face.len;
        source.compressed   = true;   // all embedded faces are stb-compressed
        return ImguiFontSourceId{ mSources.add(source) };
    };

    mDefaultSourceId    = addBuiltin(mFamily.regular);
    mBoldSourceId       = addBuiltin(mFamily.bold);
    mItalicSourceId     = addBuiltin(mFamily.italic);
    mBoldItalicSourceId = addBuiltin(mFamily.boldItalic);
    mMonoSourceId       = addBuiltin(mFamily.mono);

    // Optional CJK faces (present only for scripts compiled in). Each carries
    // its own glyph range; the source id is recorded per script and exposed
    // via cjkSourceId(). Not merged into the main/default font - opt-in, so
    // the atlas stays small until the user bakes one.
    for (const CjkFace& face : mFamily.cjk)
        mCjkSourceIds[static_cast<int>(face.script)] = addBuiltin(face.data, face.range);

    // Main HiDPI font and the secondary-context default are both built from
    // the regular face (unchanged behavior from the single-font setup).
    addMainHiDpiFont(mFamily.regular.data, mFamily.regular.len, mImguiFontSize, contentScale);
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
        bool isCjkBuiltin = false;
        for (const auto& [script, srcId] : mCjkSourceIds)
            if (srcId.id == op.sourceId.id) { isCjkBuiltin = true; break; }
        debugCheck(op.sourceId.id != mDefaultSourceId.id
                && op.sourceId.id != mBoldSourceId.id
                && op.sourceId.id != mItalicSourceId.id
                && op.sourceId.id != mBoldItalicSourceId.id
                && op.sourceId.id != mMonoSourceId.id
                && !isCjkBuiltin,
            "ImguiFontManager: cannot remove a built-in font source");
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
    addMainHiDpiFont(mFamily.regular.data, mFamily.regular.len, mImguiFontSize, contentScale);

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

std::optional<ImguiFontSourceId> ImguiFontManager::cjkSourceId(CjkScript script) const noexcept
{
    auto it = mCjkSourceIds.find(static_cast<int>(script));
    if (it == mCjkSourceIds.end()) return std::nullopt;
    return it->second;
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
