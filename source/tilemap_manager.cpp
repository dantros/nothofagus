#include "tilemap_manager.h"
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

TilemapId TilemapManager::addTilemap(Tilemap tilemap)
{
    return TilemapId{ mTilemaps.add(std::move(tilemap)) };
}

void TilemapManager::removeTilemap(TilemapId tilemapId)
{
    for (const auto& [viewIdx, viewPack] : mTilemapExplorers)
    {
        debugCheck(viewPack.view.tilemap().id != tilemapId.id,
            "Cannot remove Tilemap while a TilemapExplorer still references it — remove the view first.");
    }
    mTilemaps.remove(tilemapId.id);
}

Tilemap& TilemapManager::tilemap(TilemapId tilemapId)
{
    return mTilemaps.at(tilemapId.id);
}

const Tilemap& TilemapManager::tilemap(TilemapId tilemapId) const
{
    return mTilemaps.at(tilemapId.id);
}

TilemapExplorerId TilemapManager::addTilemapExplorer(TilemapExplorer view, Canvas& canvas)
{
    debugCheck(mTilemaps.contains(view.tilemap().id),
        "TilemapExplorer references a TilemapId not registered with this canvas.");

    TilemapExplorerPack pack(view);
    buildPoolSlots(pack, canvas);
    return TilemapExplorerId{ mTilemapExplorers.add(std::move(pack)) };
}

void TilemapManager::removeTilemapExplorer(TilemapExplorerId viewId, Canvas& canvas)
{
    TilemapExplorerPack& pack = mTilemapExplorers.at(viewId.id);
    teardownPoolSlots(pack, canvas);
    mTilemapExplorers.remove(viewId.id);
}

void TilemapManager::buildPoolSlots(TilemapExplorerPack& pack, Canvas& canvas)
{
    const Tilemap& sourceTilemap = mTilemaps.at(pack.view.tilemap().id);

    const glm::ivec2 chunkSize = sourceTilemap.chunkSize();
    const glm::ivec2 tileSize  = sourceTilemap.tileSize();
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

    const std::int8_t depthOffset = pack.view.depthOffset();

    for (std::size_t slotIdx = 0; slotIdx < slotCount; ++slotIdx)
    {
        // Clone the Tilemap's cache (atlas + palette + layers) and replace its
        // world-sized map with a chunk-sized one — slot draws are over chunkSize.
        IndirectTexture slotTexture(sourceTilemap.cacheTexture(), chunkSize);

        TextureId texId = canvas.addTexture(slotTexture);
        mExplorerManagedTextureIds.insert(texId.id);

        Bellota slotBellota(Transform(glm::vec2(0.0f, 0.0f)), texId, depthOffset);
        slotBellota.visible() = false; // hidden until per-frame pass assigns it
        BellotaId bellotaId = canvas.addBellota(slotBellota);
        mExplorerManagedBellotaIds.insert(bellotaId.id);

        pack.slots.push_back(PoolSlot{ texId, bellotaId, glm::ivec2{-1, -1}, 0 });
    }
}

void TilemapManager::teardownPoolSlots(TilemapExplorerPack& pack, Canvas& canvas)
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

TilemapExplorer& TilemapManager::tilemapExplorer(TilemapExplorerId viewId)
{
    return mTilemapExplorers.at(viewId.id).view;
}

const TilemapExplorer& TilemapManager::tilemapExplorer(TilemapExplorerId viewId) const
{
    return mTilemapExplorers.at(viewId.id).view;
}

void TilemapManager::updateExplorers(Canvas& canvas)
{
    if (mTilemapExplorers.size() == 0) return;

    const ScreenSize& screen = canvas.screenSize();
    const glm::vec2 canvasCenter{
        static_cast<float>(screen.width)  * 0.5f,
        static_cast<float>(screen.height) * 0.5f
    };

    for (auto& [viewIdx, viewPack] : mTilemapExplorers)
    {
        const TilemapId tilemapId = viewPack.view.tilemap();
        if (!mTilemaps.contains(tilemapId.id)) continue;

        // Canvas was resized since this pool was built — tear it down and
        // rebuild against the new screenSize. Fresh slots have currentWorldChunk
        // = {-1,-1}, so the chunk-sync pass below re-syncs every slot this frame.
        if (screen.width  != viewPack.poolSizedFor.width ||
            screen.height != viewPack.poolSizedFor.height)
        {
            teardownPoolSlots(viewPack, canvas);
            buildPoolSlots(viewPack, canvas);
        }

        const Tilemap& sourceTilemap = mTilemaps.at(tilemapId.id);

        const glm::ivec2 chunkSize     = sourceTilemap.chunkSize();
        const glm::ivec2 tileSize      = sourceTilemap.tileSize();
        const glm::ivec2 chunkGridSize = sourceTilemap.chunkGridSize();
        const glm::vec2  chunkPixelSize{
            static_cast<float>(chunkSize.x * tileSize.x),
            static_cast<float>(chunkSize.y * tileSize.y)
        };

        const glm::vec2 camera = viewPack.view.camera();
        const glm::vec2 worldBottomLeft = camera - canvasCenter;

        const glm::ivec2 slotOriginChunk{
            static_cast<int>(std::floor(worldBottomLeft.x / chunkPixelSize.x)) - 1,
            static_cast<int>(std::floor(worldBottomLeft.y / chunkPixelSize.y)) - 1
        };

        const std::int8_t depthOffset = viewPack.view.depthOffset();

        for (int py = 0; py < viewPack.poolGridSize.y; ++py)
        {
            for (int px = 0; px < viewPack.poolGridSize.x; ++px)
            {
                const std::size_t slotIdx =
                    static_cast<std::size_t>(py) * static_cast<std::size_t>(viewPack.poolGridSize.x) +
                    static_cast<std::size_t>(px);
                PoolSlot& slot = viewPack.slots[slotIdx];

                Bellota& slotBellota = canvas.bellota(slot.bellotaId);
                slotBellota.depthOffset() = depthOffset;

                const glm::ivec2 desired{
                    slotOriginChunk.x + px,
                    slotOriginChunk.y + py
                };
                const bool outOfWorld =
                    desired.x < 0 || desired.y < 0 ||
                    desired.x >= chunkGridSize.x || desired.y >= chunkGridSize.y;

                if (outOfWorld)
                {
                    slotBellota.visible() = false;
                    continue;
                }

                const std::uint64_t currentGen = sourceTilemap.chunkGeneration(desired);
                if (desired != slot.currentWorldChunk || currentGen != slot.syncedGeneration)
                {
                    ZoneScopedN("TilemapChunkSync");
                    IndirectTexture& slotTex = std::get<IndirectTexture>(
                        canvas.texture(slot.textureId));
                    sourceTilemap.chunkDataInto(desired, std::span<std::uint8_t>(viewPack.chunkScratch));
                    slotTex.setMapBulk(std::span<const std::uint8_t>(viewPack.chunkScratch));
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

}
