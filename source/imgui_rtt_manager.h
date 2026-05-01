#pragma once

#include "imgui_draw_callback.h"
#include "render_target.h"
#include "render_target_container.h"
#include "backends/render_backend_select.h"
#include <vector>
#include <utility>
#include <unordered_map>
#include <cstddef>
#include <memory>

struct ImGuiContext;
struct ImFontAtlas;
struct ImFont;

namespace Nothofagus
{

/// Custom deleter that destroys an ImGuiContext via ImGui::DestroyContext.
/// Allows std::unique_ptr<ImGuiContext, ...> to manage lifetime correctly
/// (DestroyContext is the only valid way to dispose of an ImGuiContext;
/// `delete` would be undefined behaviour).
struct ImGuiContextDeleter
{
    void operator()(ImGuiContext* ctx) const noexcept;
};

using ImGuiContextPtr = std::unique_ptr<ImGuiContext, ImGuiContextDeleter>;

/// Owns ImFont* handles for fonts the RTT flow uses: the default font baked
/// into newly-created secondary contexts, plus a size→ImFont cache that
/// dedupes Canvas::addImguiFont() calls. ImFonts are owned by the shared
/// ImFontAtlas; this class only stores non-owning observers.
class ImguiRttFontCache
{
public:
    /// Register the TTF buffer used by all subsequent bake() calls. Must be
    /// called before bake() / setDefaultSize(). The buffer must outlive the
    /// cache — typically a static byte array embedded in the binary.
    void setFontData(const void* data, std::size_t length) noexcept;

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

private:
    const void* mFontData{nullptr};
    std::size_t mFontDataLen{0};
    std::unordered_map<float, ImFont*> mCache;
    ImFont* mDefaultFont{nullptr};
};

/// Owns the queue of pending ImGui-RTT passes plus the per-RTT secondary
/// ImGuiContext cache. Encapsulates the lazy-create-on-first-use pattern and
/// the tear-down flow shared by removeRenderTarget() and the destructor.
class ImguiRttManager
{
public:
    ImguiRttManager(ActiveBackend& backend, RenderTargetContainer& renderTargets);

    /// Queue an ImGui draw callback to run against renderTargetId this frame.
    void enqueue(RenderTargetId renderTargetId, ImguiDrawCallback imguiDrawCallback);

    /// Drain the queue: lazy-create the secondary context per RTT, run the
    /// callback, submit through the backend's RTT pass methods.
    /// Caller must have the main ImGuiContext current on entry; this method
    /// restores it before returning.
    /// On lazy-create, the secondary context's `io.FontDefault` is set to
    /// `fonts().defaultFont()` (if non-null) so glyphs render crisp at their
    /// logical pixel height in RTT pixels (RTT pixels are game-canvas pixels
    /// — OS DPI scaling has no meaning there).
    void flushPending(float deltaTimeMS, ImFontAtlas* sharedFonts);

    /// Tear down the secondary context for one render target (no-op if absent).
    /// Safe to call even if the RTT's dRenderTargetOpt is empty.
    void releaseContext(RenderTargetId renderTargetId);

    /// Tear down every secondary context. Must run while mBackend is still
    /// alive — i.e. before mBackend.shutdown() in the destructor.
    void releaseAll();

    /// Access to the RTT-flow font cache (default font + size→ImFont dedup).
    ImguiRttFontCache&       fonts()       noexcept { return mFonts; }
    const ImguiRttFontCache& fonts() const noexcept { return mFonts; }

private:
    ActiveBackend&         mBackend;
    RenderTargetContainer& mRenderTargets;
    std::vector<std::pair<RenderTargetId, ImguiDrawCallback>> mPendingPasses;
    std::unordered_map<std::size_t /*RenderTargetId*/, ImGuiContextPtr> mContexts;
    ImguiRttFontCache mFonts;
};

}
