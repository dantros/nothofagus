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

template<TilemapLike T>
typename ExplorerManager<T>::DataId ExplorerManager<T>::add(T data)
{
    return DataId{ mData.add(std::move(data)) };
}

template<TilemapLike T>
void ExplorerManager<T>::remove(DataId id)
{
    for (const auto& [explorerIdx, explorerPack] : mExplorers)
    {
        debugCheck(explorerPack.explorer.tilemap().id != id.id,
            "Cannot remove the backing data while an Explorer still references it — remove the explorer first.");
    }
    mData.remove(id.id);
}

template<TilemapLike T>
T& ExplorerManager<T>::get(DataId id)
{
    return mData.at(id.id);
}

template<TilemapLike T>
const T& ExplorerManager<T>::get(DataId id) const
{
    return mData.at(id.id);
}

template<TilemapLike T>
typename ExplorerManager<T>::ExplorerId ExplorerManager<T>::addExplorer(Explorer<T> explorer, Canvas& canvas)
{
    debugCheck(mData.contains(explorer.tilemap().id),
        "Explorer references a backing data id not registered with this canvas.");

    ExplorerPack<T> pack(explorer);
    buildPoolSlots(pack, canvas);
    return ExplorerId{ mExplorers.add(std::move(pack)) };
}

template<TilemapLike T>
void ExplorerManager<T>::removeExplorer(ExplorerId id, Canvas& canvas)
{
    ExplorerPack<T>& pack = mExplorers.at(id.id);
    teardownPoolSlots(pack, canvas);
    mExplorers.remove(id.id);
}

template<TilemapLike T>
void ExplorerManager<T>::buildPoolSlots(ExplorerPack<T>& pack, Canvas& canvas)
{
    const T& sourceData = mData.at(pack.explorer.tilemap().id);

    const glm::ivec2 chunkSize = sourceData.chunkSize();
    const glm::ivec2 tileSize  = sourceData.tileSize();
    const glm::ivec2 chunkPixelSize{ chunkSize.x * tileSize.x, chunkSize.y * tileSize.y };
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

template<TilemapLike T>
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

template<TilemapLike T>
Explorer<T>& ExplorerManager<T>::getExplorer(ExplorerId id)
{
    return mExplorers.at(id.id).explorer;
}

template<TilemapLike T>
const Explorer<T>& ExplorerManager<T>::getExplorer(ExplorerId id) const
{
    return mExplorers.at(id.id).explorer;
}

template<TilemapLike T>
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
        const DataId dataId = explorerPack.explorer.tilemap();
        if (!mData.contains(dataId.id)) continue;

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

        const T& sourceData = mData.at(dataId.id);

        const glm::ivec2 chunkSize = sourceData.chunkSize();
        const glm::ivec2 tileSize  = sourceData.tileSize();
        const glm::vec2  chunkPixelSize{
            static_cast<float>(chunkSize.x * tileSize.x),
            static_cast<float>(chunkSize.y * tileSize.y)
        };

        const glm::vec2 camera = explorerPack.explorer.camera();
        const glm::vec2 worldBottomLeft = camera - canvasCenter;

        const glm::ivec2 slotOriginChunk{
            static_cast<int>(std::floor(worldBottomLeft.x / chunkPixelSize.x)) - 1,
            static_cast<int>(std::floor(worldBottomLeft.y / chunkPixelSize.y)) - 1
        };

        const std::int8_t depthOffset = explorerPack.explorer.depthOffset();

        for (int py = 0; py < explorerPack.poolGridSize.y; ++py)
        {
            for (int px = 0; px < explorerPack.poolGridSize.x; ++px)
            {
                const std::size_t slotIdx =
                    static_cast<std::size_t>(py) * static_cast<std::size_t>(explorerPack.poolGridSize.x) +
                    static_cast<std::size_t>(px);
                PoolSlot& slot = explorerPack.slots[slotIdx];

                Bellota& slotBellota = canvas.bellota(slot.bellotaId);
                if (slotBellota.depthOffset() != depthOffset)
                    slotBellota.depthOffset() = depthOffset;

                const glm::ivec2 desired{
                    slotOriginChunk.x + px,
                    slotOriginChunk.y + py
                };

                if (!sourceData.hasChunk(desired))
                {
                    slotBellota.visible() = false;
                    // Reset to the unassigned sentinel so the slot doesn't carry
                    // "what chunk am I painting" state while hidden — when it
                    // scrolls back into a present chunk the desired-vs-current
                    // check will trigger a fresh sync.
                    slot.currentWorldChunk = glm::ivec2{-1, -1};
                    slot.syncedGeneration  = 0;
                    continue;
                }

                const std::uint64_t currentGen = sourceData.chunkGeneration(desired);
                if (desired != slot.currentWorldChunk || currentGen != slot.syncedGeneration)
                {
                    ZoneScopedN("TilemapChunkSync");
                    IndirectTexture& slotTex = std::get<IndirectTexture>(
                        canvas.texture(slot.textureId));
                    sourceData.chunkDataInto(desired, std::span<std::uint8_t>(explorerPack.chunkScratch));
                    slotTex.setMapBulk(std::span<const std::uint8_t>(explorerPack.chunkScratch));
                    slot.currentWorldChunk = desired;
                    slot.syncedGeneration  = currentGen;
                }

                const glm::vec2 chunkCenterWorld{
                    (static_cast<float>(desired.x) + 0.5f) * chunkPixelSize.x,
                    (static_cast<float>(desired.y) + 0.5f) * chunkPixelSize.y
                };
                slotBellota.transform().location() = canvasCenter + chunkCenterWorld - camera;
                slotBellota.visible() = true;
            }
        }
    }
}

// Explicit instantiations — one per backend. Any third backend added later only
// needs a `static_assert(TilemapLike<X>);` in its source + a line here.
template class ExplorerManager<Tilemap>;
template class ExplorerManager<Sparsemap>;

}
