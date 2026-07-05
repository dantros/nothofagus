#include "imgui_draw_clone.h"

namespace Nothofagus
{

// Definition of the thread-local current-context pointer declared in imconfig.h
// (`#define GImGui GNothofagusImGuiTLS`). Zero-initialized per thread; each thread
// that uses ImGui sets it via ImGui::CreateContext / SetCurrentContext.
}

thread_local ImGuiContext* GNothofagusImGuiTLS = nullptr;

namespace Nothofagus
{

ClonedImDrawData::~ClonedImDrawData()
{
    clear();
}

void ClonedImDrawData::clear()
{
    for (ImDrawList* list : mOwnedLists)
        IM_DELETE(list);
    mOwnedLists.clear();
    mDrawData.Clear();
}

void ClonedImDrawData::cloneFrom(const ImDrawData* src)
{
    clear();
    if (src == nullptr)
        return;

    mDrawData.Valid            = src->Valid;
    mDrawData.DisplayPos       = src->DisplayPos;
    mDrawData.DisplaySize      = src->DisplaySize;
    mDrawData.FramebufferScale = src->FramebufferScale;
    mDrawData.OwnerViewport    = nullptr;
    mDrawData.Textures         = nullptr; // set render-side to the render context's list

    // Populate the ImDrawData manually rather than via AddDrawList(): the latter
    // asserts the list is a live, mid-build draw list (write pointer at the end),
    // whereas CloneOutput() returns an already-sealed copy. This is the idiom
    // used by imgui_club's threaded-rendering helper.
    mOwnedLists.reserve(static_cast<std::size_t>(src->CmdListsCount));
    for (int i = 0; i < src->CmdListsCount; ++i)
    {
        ImDrawList* cloned = src->CmdLists[i]->CloneOutput();
        mOwnedLists.push_back(cloned);
        mDrawData.CmdLists.push_back(cloned);
        mDrawData.TotalVtxCount += cloned->VtxBuffer.Size;
        mDrawData.TotalIdxCount += cloned->IdxBuffer.Size;
    }
    mDrawData.CmdListsCount = mDrawData.CmdLists.Size;
}

}
