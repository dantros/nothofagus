#include "tilemap.h"
#include "check.h"

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
    mMapSize(mapSize),
    mChunkSize(chunkSize),
    mTileSize(tileSize),
    mChunkGridSize(computeChunkGridSize(mapSize, chunkSize)),
    mPalette(palette),
    mTileGraphics(tileGraphics.begin(), tileGraphics.end()),
    mCellGrid(static_cast<std::size_t>(mapSize.x) * static_cast<std::size_t>(mapSize.y), 0),
    mChunkGenerations(static_cast<std::size_t>(mChunkGridSize.x) * static_cast<std::size_t>(mChunkGridSize.y), 0)
{
    debugCheck(mapSize.x > 0 && mapSize.y > 0, "Tilemap mapSize must be positive.");
    debugCheck(chunkSize.x > 0 && chunkSize.y > 0, "Tilemap chunkSize must be positive.");
    debugCheck(tileSize.x > 0 && tileSize.y > 0, "Tilemap tileSize must be positive.");
    debugCheck(!mTileGraphics.empty(), "Tilemap requires at least one tile graphic layer.");
    const std::size_t pixelsPerLayer = static_cast<std::size_t>(tileSize.x) * static_cast<std::size_t>(tileSize.y);
    for (const auto& layerPixels : mTileGraphics)
    {
        debugCheck(layerPixels.size() == pixelsPerLayer,
                   "Tilemap tile graphic layer size must equal tileSize.x * tileSize.y.");
    }
}

void Tilemap::setCell(glm::ivec2 worldCell, std::uint8_t layerIndex)
{
    debugCheck(worldCell.x >= 0 && worldCell.x < mMapSize.x
            && worldCell.y >= 0 && worldCell.y < mMapSize.y,
               "Tilemap::setCell coordinate out of world bounds.");
    debugCheck(static_cast<std::size_t>(layerIndex) < mTileGraphics.size(),
               "Tilemap::setCell layer index out of range of registered tile graphics.");

    const std::size_t cellIdx =
        static_cast<std::size_t>(worldCell.y) * static_cast<std::size_t>(mMapSize.x) +
        static_cast<std::size_t>(worldCell.x);
    mCellGrid[cellIdx] = layerIndex;

    const glm::ivec2 chunkPos{worldCell.x / mChunkSize.x, worldCell.y / mChunkSize.y};
    const std::size_t chunkIdx =
        static_cast<std::size_t>(chunkPos.y) * static_cast<std::size_t>(mChunkGridSize.x) +
        static_cast<std::size_t>(chunkPos.x);
    ++mChunkGenerations[chunkIdx];
}

std::uint8_t Tilemap::cell(glm::ivec2 worldCell) const
{
    debugCheck(worldCell.x >= 0 && worldCell.x < mMapSize.x
            && worldCell.y >= 0 && worldCell.y < mMapSize.y,
               "Tilemap::cell coordinate out of world bounds.");
    const std::size_t cellIdx =
        static_cast<std::size_t>(worldCell.y) * static_cast<std::size_t>(mMapSize.x) +
        static_cast<std::size_t>(worldCell.x);
    return mCellGrid[cellIdx];
}

std::vector<std::uint8_t> Tilemap::chunkData(glm::ivec2 chunkPos) const
{
    debugCheck(chunkPos.x >= 0 && chunkPos.x < mChunkGridSize.x
            && chunkPos.y >= 0 && chunkPos.y < mChunkGridSize.y,
               "Tilemap::chunkData chunk coordinate out of grid bounds.");

    const std::size_t cellsPerChunk =
        static_cast<std::size_t>(mChunkSize.x) * static_cast<std::size_t>(mChunkSize.y);
    std::vector<std::uint8_t> out(cellsPerChunk, 0);

    const int worldColStart = chunkPos.x * mChunkSize.x;
    const int worldRowStart = chunkPos.y * mChunkSize.y;
    const int worldColEnd = std::min(worldColStart + mChunkSize.x, mMapSize.x);
    const int worldRowEnd = std::min(worldRowStart + mChunkSize.y, mMapSize.y);

    for (int worldRow = worldRowStart; worldRow < worldRowEnd; ++worldRow)
    {
        const int localRow = worldRow - worldRowStart;
        const std::size_t worldRowOffset =
            static_cast<std::size_t>(worldRow) * static_cast<std::size_t>(mMapSize.x);
        const std::size_t localRowOffset =
            static_cast<std::size_t>(localRow) * static_cast<std::size_t>(mChunkSize.x);
        for (int worldCol = worldColStart; worldCol < worldColEnd; ++worldCol)
        {
            const int localCol = worldCol - worldColStart;
            out[localRowOffset + static_cast<std::size_t>(localCol)] =
                mCellGrid[worldRowOffset + static_cast<std::size_t>(worldCol)];
        }
    }

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
