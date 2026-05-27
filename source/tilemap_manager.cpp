#include "tilemap_manager.h"
#include "canvas.h"
#include "check.h"
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
    for (const auto& [viewIdx, viewPack] : mTilemapViews)
    {
        debugCheck(viewPack.view.tilemap().id != tilemapId.id,
            "Cannot remove Tilemap while a TilemapView still references it — remove the view first.");
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

TilemapViewId TilemapManager::addTilemapView(TilemapView view, Canvas& canvas)
{
    debugCheck(mTilemaps.contains(view.tilemap().id),
        "TilemapView references a TilemapId not registered with this canvas.");
    const Tilemap& sourceTilemap = mTilemaps.at(view.tilemap().id);

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

    TilemapViewPack pack(view);
    pack.poolGridSize = poolGridSize;
    const std::size_t slotCount =
        static_cast<std::size_t>(poolGridSize.x) * static_cast<std::size_t>(poolGridSize.y);
    pack.slots.reserve(slotCount);

    const auto tileGraphics = sourceTilemap.tileGraphics();
    const std::size_t layerCount = tileGraphics.size();
    const std::int8_t depthOffset = view.depthOffset();

    for (std::size_t slotIdx = 0; slotIdx < slotCount; ++slotIdx)
    {
        // Build the slot's IndirectTexture: own copy of atlas + palette,
        // chunk-sized map storage initialized to all-zero.
        IndirectTexture slotTexture(tileSize, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f), layerCount);
        slotTexture.setPallete(sourceTilemap.palette());
        for (std::size_t layerIdx = 0; layerIdx < layerCount; ++layerIdx)
        {
            slotTexture.setPixels(
                std::span<const std::uint8_t>(tileGraphics[layerIdx]),
                layerIdx);
        }
        slotTexture.setMap(chunkSize);

        TextureId texId = canvas.addTexture(slotTexture);
        mViewManagedTextureIds.insert(texId.id);

        Bellota slotBellota(Transform(glm::vec2(0.0f, 0.0f)), texId, depthOffset);
        slotBellota.visible() = false; // hidden until per-frame pass assigns it
        BellotaId bellotaId = canvas.addBellota(slotBellota);
        mViewManagedBellotaIds.insert(bellotaId.id);

        pack.slots.push_back(PoolSlot{ texId, bellotaId, glm::ivec2{-1, -1}, 0 });
    }

    return TilemapViewId{ mTilemapViews.add(std::move(pack)) };
}

void TilemapManager::removeTilemapView(TilemapViewId viewId, Canvas& canvas)
{
    TilemapViewPack& pack = mTilemapViews.at(viewId.id);

    // Untag first so the canvas's removeBellota / removeTexture debugCheck passes.
    // Tear down each slot's bellota before its texture so the usage monitor
    // moves the texture into the unused set ahead of removeTexture.
    for (const PoolSlot& slot : pack.slots)
    {
        mViewManagedBellotaIds.erase(slot.bellotaId.id);
        canvas.removeBellota(slot.bellotaId);
        mViewManagedTextureIds.erase(slot.textureId.id);
        canvas.removeTexture(slot.textureId);
    }

    mTilemapViews.remove(viewId.id);
}

TilemapView& TilemapManager::tilemapView(TilemapViewId viewId)
{
    return mTilemapViews.at(viewId.id).view;
}

const TilemapView& TilemapManager::tilemapView(TilemapViewId viewId) const
{
    return mTilemapViews.at(viewId.id).view;
}

void TilemapManager::updateViews(Canvas& canvas)
{
    if (mTilemapViews.size() == 0) return;

    const ScreenSize& screen = canvas.screenSize();
    const glm::vec2 canvasCenter{
        static_cast<float>(screen.width)  * 0.5f,
        static_cast<float>(screen.height) * 0.5f
    };

    for (auto& [viewIdx, viewPack] : mTilemapViews)
    {
        const TilemapId tilemapId = viewPack.view.tilemap();
        if (!mTilemaps.contains(tilemapId.id)) continue;
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
                    IndirectTexture& slotTex = std::get<IndirectTexture>(
                        canvas.texture(slot.textureId));
                    const auto chunkCells = sourceTilemap.chunkData(desired);
                    slotTex.setMapBulk(std::span<const std::uint8_t>(chunkCells));
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
