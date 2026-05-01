#include "imgui_rtt_manager.h"
#include "profiling.h"
#include <imgui.h>
#include <algorithm>

namespace Nothofagus
{

void ImGuiContextDeleter::operator()(ImGuiContext* ctx) const noexcept
{
    if (ctx) ImGui::DestroyContext(ctx);
}

ImguiRttManager::ImguiRttManager(ActiveBackend& backend,
                                  RenderTargetContainer& renderTargets,
                                  const void* fontData,
                                  std::size_t fontDataLen,
                                  float imguiFontSize)
    : mBackend(backend),
      mRenderTargets(renderTargets),
      mFonts(fontData, fontDataLen, imguiFontSize)
{}

void ImguiRttManager::enqueue(RenderTargetId renderTargetId, ImguiDrawCallback imguiDrawCallback)
{
    mPendingPasses.emplace_back(renderTargetId, std::move(imguiDrawCallback));
}

namespace
{

// Tear down the per-RTT ImGui state on the backend. The owning unique_ptr is
// responsible for destroying the ImGuiContext itself afterwards (on erase/clear).
// Caller is responsible for save/restore of the main context.
// Returns true if any backend shutdown work happened.
bool shutdownContextBackendIfAlive(
    std::size_t renderTargetIndex,
    ImGuiContext* rttCtx,
    ActiveBackend& backend,
    RenderTargetContainer& renderTargets)
{
    if (rttCtx == nullptr) return false;
    if (!renderTargets.contains(renderTargetIndex)) return false;
    auto& pack = renderTargets.at(renderTargetIndex);
    if (!pack.dRenderTargetOpt.has_value()) return false;
    ImGui::SetCurrentContext(rttCtx);
    backend.shutdownImguiForRenderTarget(pack.dRenderTargetOpt.value());
    return true;
}

}

void ImguiRttManager::flushPending(float deltaTimeMS, ImFontAtlas* sharedFonts)
{
    if (mPendingPasses.empty()) return;

    ImFont* rttFont = mFonts.defaultFont();

    ZoneScopedN("ImGuiRttPasses");
    ImGuiContext* mainCtx = ImGui::GetCurrentContext();

    for (auto& [renderTargetId, imguiDrawCallback] : mPendingPasses)
    {
        if (!mRenderTargets.contains(renderTargetId.id)) continue;
        RenderTargetPack& renderTargetPack = mRenderTargets.at(renderTargetId.id);
        if (!renderTargetPack.dRenderTargetOpt.has_value()) continue;
        if (!imguiDrawCallback) continue;

        const DRenderTarget& dRenderTarget = renderTargetPack.dRenderTargetOpt.value();
        const glm::vec4& clearColor        = renderTargetPack.renderTarget.mClearColor;

        // Lazy-create the secondary context for this RTT.
        ImGuiContextPtr& rttCtx = mContexts[renderTargetId.id];
        if (!rttCtx)
        {
            rttCtx.reset(ImGui::CreateContext(sharedFonts)); // shared atlas
            ImGui::SetCurrentContext(rttCtx.get());
            ImGuiIO& rttIo = ImGui::GetIO();
            rttIo.IniFilename             = nullptr;
            rttIo.BackendPlatformName     = "nothofagus_rtt_headless";
            rttIo.BackendPlatformUserData = nullptr;
            rttIo.DisplaySize = ImVec2(
                static_cast<float>(dRenderTarget.size.x),
                static_cast<float>(dRenderTarget.size.y));
            // Default to the unscaled-size RTT font so glyphs are crisp at
            // their logical pixel height regardless of OS DPI.
            if (rttFont != nullptr) rttIo.FontDefault = rttFont;
            mBackend.initImguiForRenderTarget(dRenderTarget);
        }

        ImGui::SetCurrentContext(rttCtx.get());
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(
            static_cast<float>(dRenderTarget.size.x),
            static_cast<float>(dRenderTarget.size.y));
        io.DeltaTime = std::max(deltaTimeMS * 0.001f, 1e-6f);

        mBackend.imguiNewFrameForRenderTarget(dRenderTarget);
        ImGui::NewFrame();

        imguiDrawCallback();

        ImGui::Render();

        mBackend.beginRttPass(dRenderTarget, clearColor);
        mBackend.renderImguiDrawDataToRenderTarget(ImGui::GetDrawData(), dRenderTarget);
        mBackend.endRttPass();

        ImGui::SetCurrentContext(mainCtx);
    }
    mPendingPasses.clear();
}

void ImguiRttManager::releaseContext(RenderTargetId renderTargetId)
{
    auto it = mContexts.find(renderTargetId.id);
    if (it == mContexts.end())
        return;

    ImGuiContext* mainCtx = ImGui::GetCurrentContext();
    shutdownContextBackendIfAlive(renderTargetId.id, it->second.get(), mBackend, mRenderTargets);
    ImGui::SetCurrentContext(mainCtx);

    mContexts.erase(it); // unique_ptr deleter calls ImGui::DestroyContext.
}

void ImguiRttManager::releaseAll()
{
    if (mContexts.empty()) return;

    ImGuiContext* mainCtx = ImGui::GetCurrentContext();
    for (auto& [renderTargetIndex, rttCtx] : mContexts)
        shutdownContextBackendIfAlive(renderTargetIndex, rttCtx.get(), mBackend, mRenderTargets);
    mContexts.clear(); // unique_ptr deleters call ImGui::DestroyContext for each.
    ImGui::SetCurrentContext(mainCtx);
}

void ImguiRttManager::refreshSecondaryContextDefaultFont()
{
    if (mContexts.empty()) return;

    ImFont* newDefault = mFonts.defaultFont();
    ImGuiContext* mainCtx = ImGui::GetCurrentContext();
    for (auto& [renderTargetIndex, rttCtx] : mContexts)
    {
        if (!rttCtx) continue;
        ImGui::SetCurrentContext(rttCtx.get());
        ImGui::GetIO().FontDefault = newDefault;
    }
    ImGui::SetCurrentContext(mainCtx);
}

void ImguiRttManager::drainPendingFontOps(float contentScale)
{
    if (!mFonts.hasPendingOps()) return;
    mFonts.drainPendingOpsAndRebuildAtlas(contentScale);
    refreshSecondaryContextDefaultFont();
    mBackend.rebuildImguiFontTexture();
}

}
