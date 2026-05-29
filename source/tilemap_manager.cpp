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
    for (const auto& [explorerIdx, explorerPack] : mTilemapExplorers)
    {
        debugCheck(explorerPack.explorer.tilemap().id != tilemapId.id,
            "Cannot remove Tilemap while a TilemapExplorer still references it — remove the explorer first.");
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

TilemapExplorerId TilemapManager::addTilemapExplorer(TilemapExplorer explorer, Canvas& canvas)
{
    debugCheck(mTilemaps.contains(explorer.tilemap().id),
        "TilemapExplorer references a TilemapId not registered with this canvas.");

    TilemapExplorerPack pack(explorer);
    buildPoolSlots(pack, canvas);
    return TilemapExplorerId{ mTilemapExplorers.add(std::move(pack)) };
}

void TilemapManager::removeTilemapExplorer(TilemapExplorerId explorerId, Canvas& canvas)
{
    TilemapExplorerPack& pack = mTilemapExplorers.at(explorerId.id);
    teardownPoolSlots(pack, canvas);
    mTilemapExplorers.remove(explorerId.id);
}

void TilemapManager::buildPoolSlots(TilemapExplorerPack& pack, Canvas& canvas)
{
    const Tilemap& sourceTilemap = mTilemaps.at(pack.explorer.tilemap().id);

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

    const std::int8_t depthOffset = pack.explorer.depthOffset();

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

TilemapExplorer& TilemapManager::tilemapExplorer(TilemapExplorerId explorerId)
{
    return mTilemapExplorers.at(explorerId.id).explorer;
}

const TilemapExplorer& TilemapManager::tilemapExplorer(TilemapExplorerId explorerId) const
{
    return mTilemapExplorers.at(explorerId.id).explorer;
}

void TilemapManager::updateExplorers(Canvas& canvas)
{
    if (mTilemapExplorers.size() == 0) return;

    const ScreenSize& screen = canvas.screenSize();
    const glm::vec2 canvasCenter{
        static_cast<float>(screen.width)  * 0.5f,
        static_cast<float>(screen.height) * 0.5f
    };

    for (auto& [explorerIdx, explorerPack] : mTilemapExplorers)
    {
        updateExplorer(explorerPack, canvas, screen, canvasCenter);
    }
}

void TilemapManager::updateExplorer(
    TilemapExplorerPack& explorerPack,
    Canvas& canvas,
    const ScreenSize& screen,
    const glm::vec2& canvasCenter)
{
    const TilemapId tilemapId = explorerPack.explorer.tilemap();
    if (!mTilemaps.contains(tilemapId.id)) return;

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

    const Tilemap& sourceTilemap = mTilemaps.at(tilemapId.id);

    const glm::ivec2 chunkSize     = sourceTilemap.chunkSize();
    const glm::ivec2 tileSize      = sourceTilemap.tileSize();
    const glm::ivec2 chunkGridSize = sourceTilemap.chunkGridSize();
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
    const std::span<std::uint8_t> chunkScratch(explorerPack.chunkScratch);

    for (int py = 0; py < explorerPack.poolGridSize.y; ++py)
    {
        for (int px = 0; px < explorerPack.poolGridSize.x; ++px)
        {
            const std::size_t slotIdx =
                static_cast<std::size_t>(py) * static_cast<std::size_t>(explorerPack.poolGridSize.x) +
                static_cast<std::size_t>(px);
            PoolSlot& slot = explorerPack.slots[slotIdx];

            const glm::ivec2 desired{
                slotOriginChunk.x + px,
                slotOriginChunk.y + py
            };
            exploreCell(
                slot, desired,
                canvas, sourceTilemap,
                chunkGridSize, chunkPixelSize,
                camera, canvasCenter,
                depthOffset,
                chunkScratch);
        }
    }
}

void TilemapManager::exploreCell(
    PoolSlot& slot,
    const glm::ivec2& desired,
    Canvas& canvas,
    const Tilemap& sourceTilemap,
    const glm::ivec2& chunkGridSize,
    const glm::vec2& chunkPixelSize,
    const glm::vec2& camera,
    const glm::vec2& canvasCenter,
    std::int8_t depthOffset,
    std::span<std::uint8_t> chunkScratch)
{
    Bellota& slotBellota = canvas.bellota(slot.bellotaId);
    if (slotBellota.depthOffset() != depthOffset)
        slotBellota.depthOffset() = depthOffset;

    const bool outOfWorld =
        desired.x < 0 || desired.y < 0 ||
        desired.x >= chunkGridSize.x || desired.y >= chunkGridSize.y;

    if (outOfWorld)
    {
        slotBellota.visible() = false;
        // Reset to the unassigned sentinel so the slot doesn't carry
        // "what chunk am I painting" state while hidden — when it
        // scrolls back into the world the desired-vs-current check
        // will trigger a fresh sync.
        slot.currentWorldChunk = glm::ivec2{-1, -1};
        slot.syncedGeneration  = 0;
        return;
    }

    const std::uint64_t currentGen = sourceTilemap.chunkGeneration(desired);
    if (desired != slot.currentWorldChunk || currentGen != slot.syncedGeneration)
    {
        ZoneScopedN("TilemapChunkSync");
        IndirectTexture& slotTex = std::get<IndirectTexture>(
            canvas.texture(slot.textureId));
        sourceTilemap.chunkDataInto(desired, chunkScratch);
        slotTex.setMapBulk(std::span<const std::uint8_t>(chunkScratch));
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
