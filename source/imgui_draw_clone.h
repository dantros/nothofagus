#pragma once

#include <imgui.h>
#include <vector>

namespace Nothofagus
{

/**
 * @class ClonedImDrawData
 * @brief A deep, independently-owned copy of a frame's `ImDrawData`.
 *
 * Dear ImGui's `ImDrawData` only points at draw lists owned by its
 * `ImGuiContext`, which the next `NewFrame()` overwrites. To hand a UI frame
 * from the simulation thread (which runs `NewFrame`/widgets/`Render`) to the
 * render thread (which runs `RenderDrawData`), the draw lists must be copied.
 * `cloneFrom` deep-copies each list via the upstream `ImDrawList::CloneOutput()`
 * (its own doc recommends it for exactly this multi-threaded use) and rebuilds an
 * `ImDrawData` referencing the owned copies. Storage is reused across calls.
 *
 * Ownership rule (matches the triple-buffer hand-off): every `cloneFrom`/`clear`
 * — i.e. every allocation/free of the owned lists — happens on the producer
 * (simulation) thread, which only ever touches a slot the render thread is not
 * reading. The render thread only *reads* `drawData()`.
 *
 * `Textures` is left null; the render thread points it at its own context's
 * platform texture list before `RenderDrawData`, so font-atlas uploads are
 * applied render-side without touching the producer context's texture list.
 */
class ClonedImDrawData
{
public:
    ClonedImDrawData() = default;
    ~ClonedImDrawData();

    ClonedImDrawData(const ClonedImDrawData&) = delete;
    ClonedImDrawData& operator=(const ClonedImDrawData&) = delete;

    /// Deep-copy `src` into owned storage and rebuild the local `ImDrawData`.
    void cloneFrom(const ImDrawData* src);

    /// True when a valid, non-empty frame has been cloned.
    bool hasData() const { return mDrawData.Valid && mDrawData.CmdListsCount > 0; }

    /// The cloned draw data, valid until the next `cloneFrom`/`clear`.
    ImDrawData* drawData() { return &mDrawData; }

private:
    void clear();

    ImDrawData mDrawData;
    std::vector<ImDrawList*> mOwnedLists;
};

}
