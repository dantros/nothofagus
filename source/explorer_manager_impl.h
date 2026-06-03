#pragma once

// Template implementation of ExplorerManager<T>. Internal header — only included
// from the TU that owns the explicit instantiations (frame_runner.cpp) and the
// thin explorer_manager.cpp stub. Pulling these bodies into a header lets the
// explicit instantiations live in frame_runner.cpp next to the extern template
// declarations in frame_runner.h.

#include "explorer_manager.h"
#include "canvas.h"
#include "check.h"
#include "profiling.h"
#include "screen_size.h"
#include "transform.h"
#include "texture.h"
#include "bellota.h"
#include <glm/glm.hpp>
#include <cmath>
#include <span>
#include <variant>

namespace Nothofagus
{

namespace detail
{

/// Per-explorer constants borrowed by `exploreCell` for the duration of one
/// `updateExplorer` call. All members are read-only inputs except `canvas`
/// (the mutable rendering surface) and `chunkScratch` (a reusable upload
/// buffer that lives on the owning `ExplorerPack<T>`).
template<LandType T>
struct ExplorerFrame
{
    Canvas&                 canvas;
    const T&                sourceData;
    glm::vec2               chunkPixelSize;
    glm::vec2               camera;
    glm::vec2               canvasCenter;
    std::int8_t             depthOffset;
    std::span<std::uint8_t> chunkScratch;
};

/// Per-frame work for a single pool slot: assign the desired world chunk,
/// memcpy its cells via `setMapBulk` if the chunk or its generation changed,
/// and reposition / un-hide the slot bellota.
template<LandType T>
void exploreCell(PoolSlot& slot, glm::ivec2 desired, const ExplorerFrame<T>& frame)
{
    Bellota& slotBellota = frame.canvas.bellota(slot.bellotaId);
    if (slotBellota.depthOffset() != frame.depthOffset)
        slotBellota.depthOffset() = frame.depthOffset;

    if (!frame.sourceData.chunkInBounds(desired))
    {
        slotBellota.visible() = false;
        slot.markUnassigned();
        return;
    }

    const std::uint64_t currentGen = frame.sourceData.chunkGeneration(desired);
    if (desired != slot.currentWorldChunk || currentGen != slot.syncedGeneration)
    {
        ZoneScopedN("LandChunkSync");
        IndirectTexture& slotTex = std::get<IndirectTexture>(
            frame.canvas.texture(slot.textureId));
        frame.sourceData.chunkDataInto(desired, frame.chunkScratch);
        slotTex.setMapBulk(std::span<const std::uint8_t>(frame.chunkScratch));
        slot.currentWorldChunk = desired;
        slot.syncedGeneration  = currentGen;
    }

    const glm::vec2 chunkCenterWorld{
        (static_cast<float>(desired.x) + 0.5f) * frame.chunkPixelSize.x,
        (static_cast<float>(desired.y) + 0.5f) * frame.chunkPixelSize.y
    };
    slotBellota.transform().location() = frame.canvasCenter + chunkCenterWorld - frame.camera;
    slotBellota.visible() = true;
}

}  // namespace detail

template<LandType T>
typename ExplorerManager<T>::LandId ExplorerManager<T>::add(T data)
{
    return LandId{ mData.add(std::move(data)) };
}

template<LandType T>
void ExplorerManager<T>::remove(LandId id)
{
    for (const auto& [explorerIdx, explorerPack] : mExplorers)
    {
        debugCheck(explorerPack.explorer.land().id != id.id,
            "Cannot remove the backing data while an Explorer still references it — remove the explorer first.");
    }
    mData.remove(id.id);
}

template<LandType T>
T& ExplorerManager<T>::get(LandId id)
{
    return mData.at(id.id);
}

template<LandType T>
const T& ExplorerManager<T>::get(LandId id) const
{
    return mData.at(id.id);
}

template<LandType T>
typename ExplorerManager<T>::ExplorerId ExplorerManager<T>::addExplorer(Explorer<T> explorer, Canvas& canvas)
{
    debugCheck(mData.contains(explorer.land().id),
        "Explorer references a backing data id not registered with this canvas.");

    ExplorerPack<T> pack(explorer);
    buildPoolSlots(pack, canvas);
    return ExplorerId{ mExplorers.add(std::move(pack)) };
}

template<LandType T>
void ExplorerManager<T>::removeExplorer(ExplorerId id, Canvas& canvas)
{
    ExplorerPack<T>& pack = mExplorers.at(id.id);
    teardownPoolSlots(pack, canvas);
    mExplorers.remove(id.id);
}

template<LandType T>
void ExplorerManager<T>::buildPoolSlots(ExplorerPack<T>& pack, Canvas& canvas)
{
    const T& sourceData = mData.at(pack.explorer.land().id);

    const glm::ivec2 chunkSize      = sourceData.chunkSize();
    const glm::ivec2 chunkPixelSize = sourceData.chunkPixelSize();
    const ScreenSize& screen = canvas.screenSize();
    const glm::ivec2 screenSize{
        static_cast<int>(screen.width),
        static_cast<int>(screen.height)
    };
    const glm::ivec2 poolGridSize{
        (screenSize.x + chunkPixelSize.x - 1) / chunkPixelSize.x + 2,
        (screenSize.y + chunkPixelSize.y - 1) / chunkPixelSize.y + 2
    };

    pack.poolGridSize = poolGridSize;
    pack.poolSizedFor = screen;
    pack.slots.clear();
    const std::size_t slotCount =
        static_cast<std::size_t>(poolGridSize.x) * static_cast<std::size_t>(poolGridSize.y);
    pack.slots.reserve(slotCount);
    pack.chunkScratch.resize(
        static_cast<std::size_t>(chunkSize.x) * static_cast<std::size_t>(chunkSize.y));

    const std::int8_t depthOffset = pack.explorer.depthOffset();

    for (std::size_t slotIdx = 0; slotIdx < slotCount; ++slotIdx)
    {
        // Clone the backing data's cache (atlas + palette + layers) and replace its
        // map size with a chunk-sized one — slot draws are over chunkSize.
        IndirectTexture slotTexture(sourceData.cacheTexture(), chunkSize);

        TextureId texId = canvas.addTexture(slotTexture);
        mExplorerManagedTextureIds.insert(texId.id);

        Bellota slotBellota(Transform(glm::vec2(0.0f, 0.0f)), texId, depthOffset);
        slotBellota.visible() = false; // hidden until per-frame pass assigns it
        BellotaId bellotaId = canvas.addBellota(slotBellota);
        mExplorerManagedBellotaIds.insert(bellotaId.id);

        pack.slots.push_back(PoolSlot{ texId, bellotaId, glm::ivec2{-1, -1}, 0 });
    }
}

template<LandType T>
void ExplorerManager<T>::teardownPoolSlots(ExplorerPack<T>& pack, Canvas& canvas)
{
    // Untag first so the canvas's removeBellota / removeTexture debugCheck passes.
    // Tear down each slot's bellota before its texture so the usage monitor
    // moves the texture into the unused set ahead of removeTexture.
    for (const PoolSlot& slot : pack.slots)
    {
        mExplorerManagedBellotaIds.erase(slot.bellotaId.id);
        canvas.removeBellota(slot.bellotaId);
        mExplorerManagedTextureIds.erase(slot.textureId.id);
        canvas.removeTexture(slot.textureId);
    }
    pack.slots.clear();
}

template<LandType T>
Explorer<T>& ExplorerManager<T>::getExplorer(ExplorerId id)
{
    return mExplorers.at(id.id).explorer;
}

template<LandType T>
const Explorer<T>& ExplorerManager<T>::getExplorer(ExplorerId id) const
{
    return mExplorers.at(id.id).explorer;
}

template<LandType T>
void ExplorerManager<T>::updateExplorers(Canvas& canvas)
{
    if (mExplorers.size() == 0) return;

    const ScreenSize& screen = canvas.screenSize();
    const glm::vec2 canvasCenter{
        static_cast<float>(screen.width)  * 0.5f,
        static_cast<float>(screen.height) * 0.5f
    };

    for (auto& [explorerIdx, explorerPack] : mExplorers)
    {
        updateExplorer(explorerPack, canvas, screen, canvasCenter);
    }
}

template<LandType T>
void ExplorerManager<T>::updateExplorer(
    ExplorerPack<T>& explorerPack,
    Canvas& canvas,
    const ScreenSize& screen,
    const glm::vec2& canvasCenter)
{
    const LandId landId = explorerPack.explorer.land();
    if (!mData.contains(landId.id)) return;

    // Canvas was resized since this pool was built — tear it down and
    // rebuild against the new screenSize. Fresh slots have currentWorldChunk
    // = {-1,-1}, so the chunk-sync pass below re-syncs every slot this frame.
    // Cost: N GPU texture frees + N uploads on the same frame (N = slot count).
    if (screen.width  != explorerPack.poolSizedFor.width ||
        screen.height != explorerPack.poolSizedFor.height)
    {
        teardownPoolSlots(explorerPack, canvas);
        buildPoolSlots(explorerPack, canvas);
    }

    const T& sourceData = mData.at(landId.id);

    const glm::vec2 chunkPixelSize = glm::vec2(sourceData.chunkPixelSize());

    const glm::vec2 camera = explorerPack.explorer.camera();
    const glm::vec2 worldBottomLeft = camera - canvasCenter;

    const glm::ivec2 slotOriginChunk{
        static_cast<int>(std::floor(worldBottomLeft.x / chunkPixelSize.x)) - 1,
        static_cast<int>(std::floor(worldBottomLeft.y / chunkPixelSize.y)) - 1
    };

    const detail::ExplorerFrame<T> frame{
        .canvas         = canvas,
        .sourceData     = sourceData,
        .chunkPixelSize = chunkPixelSize,
        .camera         = camera,
        .canvasCenter   = canvasCenter,
        .depthOffset    = explorerPack.explorer.depthOffset(),
        .chunkScratch   = std::span<std::uint8_t>(explorerPack.chunkScratch),
    };

    for (int py = 0; py < explorerPack.poolGridSize.y; ++py)
    {
        for (int px = 0; px < explorerPack.poolGridSize.x; ++px)
        {
            const glm::ivec2 desired{
                slotOriginChunk.x + px,
                slotOriginChunk.y + py
            };
            detail::exploreCell(explorerPack.slotAt(px, py), desired, frame);
        }
    }
}

}
