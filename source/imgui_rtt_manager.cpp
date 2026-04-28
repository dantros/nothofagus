#include "imgui_rtt_manager.h"
#include "profiling.h"
#include <imgui.h>
#include <algorithm>

namespace Nothofagus
{

ImguiRttManager::ImguiRttManager(ActiveBackend& backend, RenderTargetContainer& renderTargets)
    : mBackend(backend), mRenderTargets(renderTargets)
{}

void ImguiRttManager::enqueue(RenderTargetId renderTargetId, ImguiDrawCallback imguiDrawCallback)
{
    mPendingPasses.emplace_back(renderTargetId, std::move(imguiDrawCallback));
}

namespace
{

// Tear down the per-RTT ImGui state on the backend, then destroy the context.
// Caller is responsible for save/restore of the main context and for erasing
// the map entry. Returns true if any work happened.
bool destroyContextIfAlive(
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
    ImGui::DestroyContext(rttCtx);
    return true;
}

}

void ImguiRttManager::flushPending(float deltaTimeMS, ImFontAtlas* sharedFonts)
{
    if (mPendingPasses.empty()) return;

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
        ImGuiContext*& rttCtx = mContexts[renderTargetId.id];
        if (rttCtx == nullptr)
        {
            rttCtx = ImGui::CreateContext(sharedFonts); // shared atlas
            ImGui::SetCurrentContext(rttCtx);
            ImGuiIO& rttIo = ImGui::GetIO();
            rttIo.IniFilename             = nullptr;
            rttIo.BackendPlatformName     = "nothofagus_rtt_headless";
            rttIo.BackendPlatformUserData = nullptr;
            rttIo.DisplaySize = ImVec2(
                static_cast<float>(dRenderTarget.size.x),
                static_cast<float>(dRenderTarget.size.y));
            mBackend.initImguiForRenderTarget(dRenderTarget);
        }

        ImGui::SetCurrentContext(rttCtx);
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
    destroyContextIfAlive(renderTargetId.id, it->second, mBackend, mRenderTargets);
    ImGui::SetCurrentContext(mainCtx);

    mContexts.erase(it);
}

void ImguiRttManager::releaseAll()
{
    if (mContexts.empty()) return;

    ImGuiContext* mainCtx = ImGui::GetCurrentContext();
    for (auto& [renderTargetIndex, rttCtx] : mContexts)
        destroyContextIfAlive(renderTargetIndex, rttCtx, mBackend, mRenderTargets);
    mContexts.clear();
    ImGui::SetCurrentContext(mainCtx);
}

}
