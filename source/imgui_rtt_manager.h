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
    ImguiRttManager(ActiveBackend& backend, RenderTargetContainer& renderTargets);

    /// Queue an ImGui draw callback to run against renderTargetId this frame.
    void enqueue(RenderTargetId renderTargetId, ImguiDrawCallback imguiDrawCallback);

    /// Drain the queue: lazy-create the secondary context per RTT, run the
    /// callback, submit through the backend's RTT pass methods.
    /// Caller must have the main ImGuiContext current on entry; this method
    /// restores it before returning.
    void flushPending(float deltaTimeMS, ImFontAtlas* sharedFonts);

    /// Tear down the secondary context for one render target (no-op if absent).
    /// Safe to call even if the RTT's dRenderTargetOpt is empty.
    void releaseContext(RenderTargetId renderTargetId);

    /// Tear down every secondary context. Must run while mBackend is still
    /// alive — i.e. before mBackend.shutdown() in the destructor.
    void releaseAll();

private:
    ActiveBackend&         mBackend;
    RenderTargetContainer& mRenderTargets;
    std::vector<std::pair<RenderTargetId, ImguiDrawCallback>> mPendingPasses;
    std::unordered_map<std::size_t /*RenderTargetId*/, ImGuiContextPtr> mContexts;
};

}
