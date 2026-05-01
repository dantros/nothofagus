#pragma once

#include "imgui_font_id.h"
#include "indexed_container.h"
#include <unordered_map>
#include <vector>
#include <optional>
#include <cstddef>

struct ImFont;

namespace Nothofagus
{

/// Owns the entire ImGui-font lifecycle for a Canvas:
/// - the IndexedContainer of FontEntry (one entry per uniquely-baked size,
///   keyed by the public ImguiFontId);
/// - the size→id dedup map so repeat bake(sameSize) calls return the same id;
/// - the secondary-context default font's id;
/// - the deferred queue of bake/remove ops + the atlas-rebuild flow that
///   drains them.
///
/// ImFont objects themselves are owned by the shared ImFontAtlas; entries
/// here only hold non-owning observer pointers, and rebakeAll() patches them
/// in place when the atlas is rebuilt without changing ids or entry slots.
class ImguiFontManager
{
public:
    /// Bind the TTF buffer + the logical font size at construction. The
    /// buffer must outlive the manager (typically a static byte array
    /// embedded in the binary). `imguiFontSize` is retained for the main
    /// HiDPI font's recipe used by initialize() and the atlas rebuild.
    ImguiFontManager(const void* fontData,
                     std::size_t fontDataLen,
                     float       imguiFontSize) noexcept;

    /// One-time setup. Adds the main HiDPI font to the shared atlas at
    /// `imguiFontSize * contentScale * contentScale`, then bakes a font at
    /// the unscaled `imguiFontSize` and registers it as the secondary-context
    /// default. Call from CanvasImpl's constructor body after backend
    /// initImGuiRenderer.
    void initialize(float contentScale);

    /// User-facing bake. Calls getOrCreate(sizePx); if the entry is pending
    /// (atlas was locked when getOrCreate ran), enqueues a Bake op so the
    /// next drain rebakes it. Returns the stable id.
    ImguiFontId bake(float sizePx);

    /// User-facing remove. Always enqueues a Remove op — the actual entry
    /// drop + atlas rebuild happens at the next drainPendingOpsAndRebuildAtlas.
    void remove(ImguiFontId id);

    /// True if there are queued ops awaiting drain.
    bool hasPendingOps() const noexcept;

    /// Drain entry point. Apply pending Remove ops, ImFontAtlas::Clear(),
    /// re-add the main HiDPI font (imguiFontSize * contentScale²), then
    /// rebakeAll() so every surviving entry gets a fresh ImFont*. Does NOT
    /// touch secondary contexts or the GPU font texture — those are
    /// ImguiRttManager's responsibility.
    void drainPendingOpsAndRebuildAtlas(float contentScale);

    // --- Cache surface (id-based, atlas-locking-aware) -----------------------

    /// Get the id for sizePx, creating a new entry if no font has been baked
    /// at this size yet. On creation: if the atlas is unlocked, bakes
    /// immediately and stores the ImFont*; if locked, leaves currentImFont
    /// null — the caller (typically `bake`) is responsible for ensuring a
    /// rebuild eventually fills it in.
    /// Repeat calls with the same sizePx return the same id.
    ImguiFontId getOrCreate(float sizePx);

    /// Resolve an id to its current ImFont*. Returns nullptr when the id
    /// is unknown / removed, OR when the entry exists but its bake is
    /// still pending.
    ImFont* get(ImguiFontId id) const noexcept;

    /// True if the id is currently registered in the container (regardless
    /// of bake-pending state).
    bool contains(ImguiFontId id) const noexcept;

    /// Walk every entry and (re-)bake it from the bound TTF buffer, updating
    /// each entry's currentImFont. Used after ImFontAtlas::Clear() to
    /// repopulate the atlas + repair every cached pointer in one pass.
    /// Ids and entry slots are preserved.
    void rebakeAll();

    /// Bake (if needed) at sizePx and register that id as the secondary-
    /// context default. Idempotent. Returns the id.
    ImguiFontId setDefaultSize(float sizePx);

    /// Current default ImFont*, or nullptr when no default set, the default
    /// id was removed, or the default's bake is still pending.
    ImFont* defaultFont() const noexcept;

    /// The id registered via setDefaultSize, or std::nullopt if no default
    /// has been set or the default's entry has since been removed.
    std::optional<ImguiFontId> defaultFontId() const noexcept { return mDefaultFontId; }

private:
    struct FontEntry
    {
        float   sizePx;
        ImFont* currentImFont;  // null while a deferred bake is pending
    };

    struct PendingFontOp
    {
        enum class Kind { Bake, Remove };
        Kind        kind;
        ImguiFontId id;
    };

    /// Calls ImGui::GetIO().Fonts->AddFontFromMemoryTTF with mFontData/Len
    /// at sizePx and FontDataOwnedByAtlas=false. Asserts non-null.
    ImFont* bakeOne(float sizePx) const;

    /// Drop the entry for id from mFonts + mSizeToId. Asserts id is valid.
    /// Clears mDefaultFontId if it referred to the dropped entry. Does NOT
    /// touch the atlas — used by the drain after pending Remove ops have
    /// been collected, before the atlas Clear.
    void dropEntry(ImguiFontId id);

    const void* const mFontData;
    const std::size_t mFontDataLen;
    const float       mImguiFontSize;

    IndexedContainer<FontEntry>            mFonts;
    std::unordered_map<float, ImguiFontId> mSizeToId;
    std::optional<ImguiFontId>             mDefaultFontId;
    std::vector<PendingFontOp>             mPendingFontOps;
};

}
