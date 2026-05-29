#include "tilemap.h"
#include "check.h"
#include <algorithm>
#include <cstdint>
#include <limits>

namespace Nothofagus
{

static glm::ivec2 computeChunkGridSize(glm::ivec2 mapSize, glm::ivec2 chunkSize)
{
    return {
        (mapSize.x + chunkSize.x - 1) / chunkSize.x,
        (mapSize.y + chunkSize.y - 1) / chunkSize.y
    };
}

Tilemap::Tilemap(glm::ivec2 mapSize,
                 glm::ivec2 chunkSize,
                 glm::ivec2 tileSize,
                 const ColorPallete& palette,
                 std::span<const std::vector<std::uint8_t>> tileGraphics):
    mCache(tileSize, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f), tileGraphics.size()),
    mChunkSize(chunkSize),
    mChunkGridSize(computeChunkGridSize(mapSize, chunkSize)),
    mChunkGenerations(static_cast<std::size_t>(mChunkGridSize.x) * static_cast<std::size_t>(mChunkGridSize.y), 0)
{
    debugCheck(mapSize.x > 0 && mapSize.y > 0, "Tilemap mapSize must be positive.");
    debugCheck(chunkSize.x > 0 && chunkSize.y > 0, "Tilemap chunkSize must be positive.");
    debugCheck(tileSize.x > 0 && tileSize.y > 0, "Tilemap tileSize must be positive.");
    debugCheck(!tileGraphics.empty(), "Tilemap requires at least one tile graphic layer.");

    // One-time bound: with chunkSize * tileSize asserted to fit in int here, the
    // per-frame chunk-pixel math in tilemap_manager.cpp can stay in plain int.
    debugCheck(
        static_cast<std::int64_t>(chunkSize.x) * static_cast<std::int64_t>(tileSize.x) <= std::numeric_limits<int>::max()
     && static_cast<std::int64_t>(chunkSize.y) * static_cast<std::int64_t>(tileSize.y) <= std::numeric_limits<int>::max(),
        "Tilemap chunkSize * tileSize exceeds int range.");

    const std::size_t pixelsPerLayer = static_cast<std::size_t>(tileSize.x) * static_cast<std::size_t>(tileSize.y);

    mCache.setPallete(palette);
    for (std::size_t i = 0; i < tileGraphics.size(); ++i)
    {
        debugCheck(tileGraphics[i].size() == pixelsPerLayer,
                   "Tilemap tile graphic layer size must equal tileSize.x * tileSize.y.");
        mCache.setPixels(std::span<const std::uint8_t>(tileGraphics[i]), i);
    }
    mCache.setMap(mapSize);

    // The cache's dirty flags will get set by setCell/setMapBulk over the Tilemap's
    // lifetime but never cleared — there's no GPU upload pass for the cache itself.
    // Functionally harmless; the chunk pool textures own the GPU side.
}

bool Tilemap::inBounds(glm::ivec2 worldCell) const
{
    const glm::ivec2 size = mCache.mapSize();
    return worldCell.x >= 0 && worldCell.x < size.x
        && worldCell.y >= 0 && worldCell.y < size.y;
}

bool Tilemap::chunkInBounds(glm::ivec2 chunkPos) const
{
    return chunkPos.x >= 0 && chunkPos.x < mChunkGridSize.x
        && chunkPos.y >= 0 && chunkPos.y < mChunkGridSize.y;
}

void Tilemap::setCell(glm::ivec2 worldCell, std::uint8_t layerIndex)
{
    debugCheck(inBounds(worldCell),
               "Tilemap::setCell coordinate out of world bounds.");
    debugCheck(static_cast<std::size_t>(layerIndex) < mCache.layers(),
               "Tilemap::setCell layer index out of range of registered tile graphics.");

    mCache.setCell(worldCell.x, worldCell.y, layerIndex);

    const glm::ivec2 chunkPos{worldCell.x / mChunkSize.x, worldCell.y / mChunkSize.y};
    const std::size_t chunkIdx =
        static_cast<std::size_t>(chunkPos.y) * static_cast<std::size_t>(mChunkGridSize.x) +
        static_cast<std::size_t>(chunkPos.x);
    ++mChunkGenerations[chunkIdx];
}

std::uint8_t Tilemap::cell(glm::ivec2 worldCell) const
{
    debugCheck(inBounds(worldCell),
               "Tilemap::cell coordinate out of world bounds.");
    return mCache.cell(worldCell.x, worldCell.y);
}

void Tilemap::chunkDataInto(glm::ivec2 chunkPos, std::span<std::uint8_t> out) const
{
    debugCheck(chunkPos.x >= 0 && chunkPos.x < mChunkGridSize.x
            && chunkPos.y >= 0 && chunkPos.y < mChunkGridSize.y,
               "Tilemap::chunkDataInto chunk coordinate out of grid bounds.");
    const std::size_t cellsPerChunk =
        static_cast<std::size_t>(mChunkSize.x) * static_cast<std::size_t>(mChunkSize.y);
    debugCheck(out.size() == cellsPerChunk,
               "Tilemap::chunkDataInto output span size must equal chunkSize.x * chunkSize.y.");

    std::fill(out.begin(), out.end(), static_cast<std::uint8_t>(0));

    const glm::ivec2 mapSize = mCache.mapSize();
    const std::span<const std::uint8_t> cells = mCache.mapData();

    const int worldColStart = chunkPos.x * mChunkSize.x;
    const int worldRowStart = chunkPos.y * mChunkSize.y;
    const int worldColEnd = std::min(worldColStart + mChunkSize.x, mapSize.x);
    const int worldRowEnd = std::min(worldRowStart + mChunkSize.y, mapSize.y);

    for (int worldRow = worldRowStart; worldRow < worldRowEnd; ++worldRow)
    {
        const int localRow = worldRow - worldRowStart;
        const std::size_t worldRowOffset =
            static_cast<std::size_t>(worldRow) * static_cast<std::size_t>(mapSize.x);
        const std::size_t localRowOffset =
            static_cast<std::size_t>(localRow) * static_cast<std::size_t>(mChunkSize.x);
        for (int worldCol = worldColStart; worldCol < worldColEnd; ++worldCol)
        {
            const int localCol = worldCol - worldColStart;
            out[localRowOffset + static_cast<std::size_t>(localCol)] =
                cells[worldRowOffset + static_cast<std::size_t>(worldCol)];
        }
    }
}

std::vector<std::uint8_t> Tilemap::chunkData(glm::ivec2 chunkPos) const
{
    const std::size_t cellsPerChunk =
        static_cast<std::size_t>(mChunkSize.x) * static_cast<std::size_t>(mChunkSize.y);
    std::vector<std::uint8_t> out(cellsPerChunk);
    chunkDataInto(chunkPos, std::span<std::uint8_t>(out));
    return out;
}

std::uint64_t Tilemap::chunkGeneration(glm::ivec2 chunkPos) const
{
    debugCheck(chunkPos.x >= 0 && chunkPos.x < mChunkGridSize.x
            && chunkPos.y >= 0 && chunkPos.y < mChunkGridSize.y,
               "Tilemap::chunkGeneration chunk coordinate out of grid bounds.");
    const std::size_t chunkIdx =
        static_cast<std::size_t>(chunkPos.y) * static_cast<std::size_t>(mChunkGridSize.x) +
        static_cast<std::size_t>(chunkPos.x);
    return mChunkGenerations[chunkIdx];
}

}
