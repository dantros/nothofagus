#include "imgui_rtt_manager.h"
#include "imgui_draw_clone.h"   // ClonedImDrawData (complete type for the clones)
#include "profiling.h"
#include <imgui.h>
#include <algorithm>
#include <memory>

namespace Nothofagus
{

void ImGuiContextDeleter::operator()(ImGuiContext* ctx) const noexcept
{
    if (ctx) ImGui::DestroyContext(ctx);
}

ImguiRttManager::ImguiRttManager(ActiveBackend& backend,
                                  RenderTargetContainer& renderTargets,
                                  const EmbeddedFontFamily& family,
                                  float imguiFontSize)
    : mBackend(backend),
      mRenderTargets(renderTargets),
      mFonts(family, imguiFontSize)
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

void ImguiRttManager::produceClones(float deltaTimeMS, ImFontAtlas* sharedFonts,
                                    std::vector<RttImguiClone>& out)
{
    if (mPendingPasses.empty())
    {
        out.clear();
        return;
    }

    ImFont* rttFont = mFonts.defaultFont();

    ZoneScopedN("ImGuiRttProduce");
    ImGuiContext* callerCtx = ImGui::GetCurrentContext();

    std::size_t slot = 0;
    for (auto& [renderTargetId, imguiDrawCallback] : mPendingPasses)
    {
        if (!mRenderTargets.contains(renderTargetId.id)) continue;
        RenderTargetPack& renderTargetPack = mRenderTargets.at(renderTargetId.id);
        if (!renderTargetPack.dRenderTargetOpt.has_value()) continue;
        if (!imguiDrawCallback) continue;

        const glm::ivec2 size = renderTargetPack.dRenderTargetOpt.value().size;

        // Lazy-create the secondary context CPU-side only — no backend renderer init here
        // (that is render-side in replayClones). Mirrors mSimUiContext: shared atlas, headless
        // IO, RendererHasTextures so the shared atlas uploads render-side from the cloned data.
        ImGuiContextPtr& rttCtx = mContexts[renderTargetId.id];
        if (!rttCtx)
        {
            rttCtx.reset(ImGui::CreateContext(sharedFonts)); // shared atlas
            ImGui::SetCurrentContext(rttCtx.get());
            ImGuiIO& rttIo = ImGui::GetIO();
            rttIo.IniFilename             = nullptr;
            rttIo.BackendPlatformName     = "nothofagus_rtt_headless";
            rttIo.BackendPlatformUserData = nullptr;
            rttIo.BackendFlags |= ImGuiBackendFlags_RendererHasTextures; // atlas owner is render-side
            // Default to the unscaled-size RTT font so glyphs are crisp at
            // their logical pixel height regardless of OS DPI.
            if (rttFont != nullptr) rttIo.FontDefault = rttFont;
        }

        ImGui::SetCurrentContext(rttCtx.get());
        ImGuiIO& io = ImGui::GetIO();
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
        io.DisplaySize = ImVec2(static_cast<float>(size.x), static_cast<float>(size.y));
        io.DeltaTime = std::max(deltaTimeMS * 0.001f, 1e-6f);

        ImGui::NewFrame();
        imguiDrawCallback();          // the user's ImGui code, on the sim thread
        ImGui::Render();

        // Reuse the snapshot slot's clone storage across frames (producer owns the write slot).
        if (slot >= out.size())
            out.emplace_back();
        RttImguiClone& clone = out[slot];
        clone.target = renderTargetId;
        if (!clone.ui)
            clone.ui = std::make_unique<ClonedImDrawData>();
        clone.ui->cloneFrom(ImGui::GetDrawData());
        ++slot;
    }

    out.resize(slot);                 // drop unused slots (frees their clones)
    ImGui::SetCurrentContext(callerCtx);
    mPendingPasses.clear();
}

void ImguiRttManager::replayClones(const std::vector<RttImguiClone>& clones)
{
    if (clones.empty()) return;

    ZoneScopedN("ImGuiRttReplay");
    ImGuiContext* callerCtx = ImGui::GetCurrentContext();

    for (const RttImguiClone& clone : clones)
    {
        if (!clone.ui || !clone.ui->hasData()) continue;
        if (!mRenderTargets.contains(clone.target.id)) continue;
        RenderTargetPack& renderTargetPack = mRenderTargets.at(clone.target.id);
        if (!renderTargetPack.dRenderTargetOpt.has_value()) continue;

        auto ctxIt = mContexts.find(clone.target.id);
        if (ctxIt == mContexts.end() || !ctxIt->second) continue; // context is created sim-side

        const DRenderTarget& dRenderTarget = renderTargetPack.dRenderTargetOpt.value();
        const glm::vec4& clearColor        = renderTargetPack.renderTarget.mClearColor;

        ImGui::SetCurrentContext(ctxIt->second.get());

        // Lazy-init the per-RTT ImGui renderer backend once, render-side (GPU).
        if (mBackendInited.insert(clone.target.id).second)
            mBackend.initImguiForRenderTarget(dRenderTarget);

        // Render-side renderer NewFrame: creates the backend's device objects (GL shader/VBO)
        // on first use and processes the shared-atlas texture queue. Independent of core
        // ImGui::NewFrame (which ran sim-side); must run before RenderDrawData.
        mBackend.imguiNewFrameForRenderTarget(dRenderTarget);

        // Point the clone at this context's platform texture list so RenderDrawData applies
        // the shared-atlas upload here (cloneFrom left Textures null) — mirrors the main path.
        ImDrawData* drawData = clone.ui->drawData();
        drawData->Textures = &ImGui::GetPlatformIO().Textures;

        mBackend.beginRttPass(dRenderTarget, clearColor);
        mBackend.renderImguiDrawDataToRenderTarget(drawData, dRenderTarget);
        mBackend.endRttPass();
    }

    ImGui::SetCurrentContext(callerCtx);
}

void ImguiRttManager::releaseContext(RenderTargetId renderTargetId)
{
    auto it = mContexts.find(renderTargetId.id);
    if (it == mContexts.end())
        return;

    ImGuiContext* mainCtx = ImGui::GetCurrentContext();
    // Only tear down the renderer backend if replayClones actually initialized it (the context
    // is created sim-side, the backend lazily render-side; an un-replayed context has none).
    if (mBackendInited.erase(renderTargetId.id) > 0)
        shutdownContextBackendIfAlive(renderTargetId.id, it->second.get(), mBackend, mRenderTargets);
    ImGui::SetCurrentContext(mainCtx);

    mContexts.erase(it); // unique_ptr deleter calls ImGui::DestroyContext.
}

void ImguiRttManager::releaseAll()
{
    if (mContexts.empty()) return;

    ImGuiContext* mainCtx = ImGui::GetCurrentContext();
    for (auto& [renderTargetIndex, rttCtx] : mContexts)
        if (mBackendInited.count(renderTargetIndex) > 0)
            shutdownContextBackendIfAlive(renderTargetIndex, rttCtx.get(), mBackend, mRenderTargets);
    mBackendInited.clear();
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

void ImguiRttManager::drainPendingFontOps()
{
    if (!mFonts.hasPendingOps()) return;
    mFonts.drainPendingOpsAndRebuildAtlas();
    refreshSecondaryContextDefaultFont();
    mBackend.rebuildImguiFontTexture();
}

}
