#pragma once

#include "imgui_font_id.h"
#include "imgui_font_source_id.h"
#include "indexed_container.h"
#include <unordered_map>
#include <vector>
#include <span>
#include <optional>
#include <cstddef>
#include <utility>

struct ImFont;

namespace Nothofagus
{

/// Owns the entire ImGui-font lifecycle for a Canvas:
/// - the IndexedContainer of FontSource (one entry per registered TTF buffer,
///   keyed by ImguiFontSourceId; the embedded default font is registered as
///   the first source by initialize());
/// - the IndexedContainer of FontEntry (one entry per uniquely-baked
///   (sourceId, sizePx) pair, keyed by the public ImguiFontId);
/// - the (sourceId, sizePx) -> id dedup map so repeat bake calls return the
///   same id;
/// - the secondary-context default font's id;
/// - the deferred queue of bake/remove/removeSource ops + the atlas-rebuild
///   flow that drains them.
///
/// ImFont objects themselves are owned by the shared ImFontAtlas; entries
/// here only hold non-owning observer pointers, and rebakeAll() patches them
/// in place when the atlas is rebuilt without changing ids or entry slots.
class ImguiFontManager
{
public:
    /// Bind the embedded default TTF buffer + the logical font size at
    /// construction. The buffer must outlive the manager (typically a static
    /// byte array embedded in the binary). `imguiFontSize` is retained for
    /// the main HiDPI font's recipe used by initialize() and the atlas
    /// rebuild.
    ImguiFontManager(const void* fontData,
                     std::size_t fontDataLen,
                     float       imguiFontSize) noexcept;

    /// One-time setup. Registers the bound TTF as the default font source
    /// (its id is exposed via defaultSourceId()), adds the main HiDPI font
    /// to the shared atlas at `imguiFontSize * contentScale * contentScale`,
    /// then bakes a font at the unscaled `imguiFontSize` from the default
    /// source and registers it as the secondary-context default. Call from
    /// CanvasImpl's constructor body after backend initImGuiRenderer.
    void initialize(float contentScale);

    /// True if there are queued ops awaiting drain.
    bool hasPendingOps() const noexcept;

    /// Drain entry point. Apply pending RemoveSource ops (cascade-drop every
    /// entry attributed to each victim source, then drop the source itself),
    /// apply pending Remove ops, ImFontAtlas::Clear(), re-add the main HiDPI
    /// font (imguiFontSize * contentScale^2), then rebakeAll() so every
    /// surviving entry gets a fresh ImFont*. Does NOT touch secondary
    /// contexts or the GPU font texture - those are ImguiRttManager's
    /// responsibility.
    void drainPendingOpsAndRebuildAtlas(float contentScale);

    // --- Per-id surface (atlas-locking-aware) -------------------------------

    /// User-facing remove. Always enqueues a Remove op - the actual entry
    /// drop + atlas rebuild happens at the next drainPendingOpsAndRebuildAtlas.
    void remove(ImguiFontId id);

    /// Resolve an id to its current ImFont*. Returns nullptr when the id
    /// is unknown / removed, OR when the entry exists but its bake is
    /// still pending.
    ImFont* get(ImguiFontId id) const noexcept;

    /// True if the id is currently registered in the container (regardless
    /// of bake-pending state).
    bool contains(ImguiFontId id) const noexcept;

    /// Walk every entry and (re-)bake it from its attributed FontSource,
    /// updating each entry's currentImFont. Used after ImFontAtlas::Clear()
    /// to repopulate the atlas + repair every cached pointer in one pass.
    /// Ids and entry slots are preserved.
    void rebakeAll();

    /// Current default ImFont*, or nullptr when no default set, the default
    /// id was removed, or the default's bake is still pending.
    ImFont* defaultFont() const noexcept;

    /// The id registered via setDefaultSize, or std::nullopt if no default
    /// has been set or the default's entry has since been removed.
    std::optional<ImguiFontId> defaultFontId() const noexcept { return mDefaultFontId; }

    // --- Multi-source surface (the only bake path) --------------------------

    /// Register a user-supplied TTF. Bytes are copied so the caller's buffer
    /// can be released immediately. Returns a stable ImguiFontSourceId.
    /// Pure registration - no atlas mutation here.
    ImguiFontSourceId addSource(std::span<const std::byte> ttfBytes,
                                GlyphRange glyphRange);

    /// Schedules cascade-removal of every entry attributed to sourceId AND
    /// the source itself. Always deferred - internally enqueues a
    /// RemoveSource op so the next drain runs the full Clear+rebake pass.
    /// Asserts (during the drain) that sourceId is registered AND is not
    /// the default source.
    void removeSource(ImguiFontSourceId sourceId);

    /// True if sourceId is currently registered.
    bool containsSource(ImguiFontSourceId sourceId) const noexcept;

    /// User-facing bake. Calls getOrCreate(sourceId, sizePx); if the entry
    /// is pending (atlas was locked when getOrCreate ran), enqueues a Bake
    /// op so the next drain rebakes it. Returns the stable id.
    ImguiFontId bake(ImguiFontSourceId sourceId, float sizePx);

    /// Get the id for (sourceId, sizePx), creating a new entry if no font
    /// has been baked at this (source, size) pair yet. On creation: if the
    /// atlas is unlocked, bakes immediately and stores the ImFont*; if
    /// locked, leaves currentImFont null - the caller (typically `bake`)
    /// is responsible for ensuring a rebuild eventually fills it in.
    /// Repeat calls with the same (sourceId, sizePx) return the same id.
    ImguiFontId getOrCreate(ImguiFontSourceId sourceId, float sizePx);

    /// Bake `sizePx` against the default source and register the resulting
    /// id as the secondary-context default. Used internally by initialize().
    /// Idempotent. Returns the id.
    ImguiFontId setDefaultSize(float sizePx);

    /// Id of the default source - the embedded TTF registered in
    /// initialize(). Stable for the manager's lifetime.
    ImguiFontSourceId defaultSourceId() const noexcept { return mDefaultSourceId; }

private:
    struct FontSource
    {
        std::vector<std::byte> ttfData;          // owned bytes (user-added)
        GlyphRange             glyphRange{GlyphRange::Default};
        const void*            externalData{nullptr};   // non-null only for the default Roboto blob
        std::size_t            externalLen{0};

        const void* dataPtr() const noexcept { return externalData ? externalData : ttfData.data(); }
        std::size_t dataLen() const noexcept { return externalData ? externalLen : ttfData.size(); }
    };

    struct FontEntry
    {
        float             sizePx;
        ImguiFontSourceId sourceId;
        ImFont*           currentImFont;  // null while a deferred bake is pending
    };

    /// Dedup key folds (sourceId, sizePx) into one map slot.
    using DedupKey = std::pair<std::size_t, float>;
    struct DedupHash
    {
        std::size_t operator()(const DedupKey& key) const noexcept;
    };

    struct PendingFontOp
    {
        enum class Kind { Bake, Remove, RemoveSource };
        Kind              kind;
        ImguiFontId       id;             // for Bake / Remove
        ImguiFontSourceId sourceId;       // for RemoveSource
    };

    /// Calls ImGui::GetIO().Fonts->AddFontFromMemoryTTF on the source's
    /// buffer at sizePx, with FontDataOwnedByAtlas=false and the right
    /// glyph-ranges pointer for source.glyphRange. Asserts non-null.
    ImFont* bakeOne(const FontSource& source, float sizePx) const;

    /// Drop the entry for id from mFonts + mDedup. Asserts id is valid.
    /// Clears mDefaultFontId if it referred to the dropped entry. Does NOT
    /// touch the atlas - used by the drain after pending Remove ops have
    /// been collected, before the atlas Clear.
    void dropEntry(ImguiFontId id);

    /// Two-pass cascade drop: collect all entry ids attributed to sourceId,
    /// then dropEntry each one. Used by the drain when applying RemoveSource.
    void dropEntriesForSource(ImguiFontSourceId sourceId);

    const void* const mFontData;
    const std::size_t mFontDataLen;
    const float       mImguiFontSize;

    IndexedContainer<FontSource>                          mSources;
    IndexedContainer<FontEntry>                           mFonts;
    std::unordered_map<DedupKey, ImguiFontId, DedupHash>  mDedup;
    std::optional<ImguiFontId>                            mDefaultFontId;
    ImguiFontSourceId                                     mDefaultSourceId{};
    std::vector<PendingFontOp>                            mPendingFontOps;
};

}
