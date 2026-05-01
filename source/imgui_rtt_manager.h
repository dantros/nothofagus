#pragma once

#include "imgui_draw_callback.h"
#include "imgui_font_manager.h"
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

/// Owns the queue of pending ImGui-RTT passes plus the per-RTT secondary
/// ImGuiContext cache. Encapsulates the lazy-create-on-first-use pattern and
/// the tear-down flow shared by removeRenderTarget() and the destructor.
class ImguiRttManager
{
public:
    ImguiRttManager(ActiveBackend& backend,
                    RenderTargetContainer& renderTargets,
                    const void* fontData,
                    std::size_t fontDataLen,
                    float imguiFontSize);

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

    /// Walk every alive secondary ImGuiContext and reset its io.FontDefault
    /// to fonts().defaultFont(). Saves/restores the main ImGuiContext.
    /// Called during atlas rebuild — the previous FontDefault pointers
    /// became stale when the atlas was Cleared.
    void refreshSecondaryContextDefaultFont();

    /// Drain the font manager's queued ops + rebuild the atlas + refresh
    /// secondary contexts + drop the GPU font texture so the next NewFrame
    /// re-uploads it. No-op when the font manager has no pending ops.
    /// Called by Canvas::CanvasImpl::runOneFrame at the top of each frame.
    void drainPendingFontOps(float contentScale);

    /// Access to the canvas-wide ImGui font manager (main HiDPI font + RTT
    /// default + user-baked sizes; deferred bake/remove queue + atlas rebuild).
    ImguiFontManager&       fonts()       noexcept { return mFonts; }
    const ImguiFontManager& fonts() const noexcept { return mFonts; }

private:
    ActiveBackend&         mBackend;
    RenderTargetContainer& mRenderTargets;
    std::vector<std::pair<RenderTargetId, ImguiDrawCallback>> mPendingPasses;
    std::unordered_map<std::size_t /*RenderTargetId*/, ImGuiContextPtr> mContexts;
    ImguiFontManager mFonts;
};

}
