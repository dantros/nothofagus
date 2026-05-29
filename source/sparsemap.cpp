#include "sparsemap.h"
#include "explorer.h"
#include "check.h"
#include <algorithm>
#include <cstdint>
#include <limits>

namespace Nothofagus
{

// Concept conformance: if a `TilemapLike` requirement breaks the build at this line,
// the missing/changed method shows up clearly instead of as a generic template error.
static_assert(TilemapLike<Sparsemap>);

Sparsemap::Sparsemap(glm::ivec2 chunkSize,
                     glm::ivec2 tileSize,
                     const ColorPallete& palette,
                     std::span<const std::vector<std::uint8_t>> tileGraphics):
    mCacheTemplate(tileSize, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f), tileGraphics.size()),
    mChunkSize(chunkSize)
{
    debugCheck(chunkSize.x > 0 && chunkSize.y > 0, "Sparsemap chunkSize must be positive.");
    debugCheck(tileSize.x > 0 && tileSize.y > 0, "Sparsemap tileSize must be positive.");
    debugCheck(!tileGraphics.empty(), "Sparsemap requires at least one tile graphic layer.");

    // Mirrors the one-time bound from `Tilemap`: with chunkSize * tileSize fitting in int,
    // the per-frame chunk-pixel math in explorer_manager.cpp can stay in plain int.
    debugCheck(
        static_cast<std::int64_t>(chunkSize.x) * static_cast<std::int64_t>(tileSize.x) <= std::numeric_limits<int>::max()
     && static_cast<std::int64_t>(chunkSize.y) * static_cast<std::int64_t>(tileSize.y) <= std::numeric_limits<int>::max(),
        "Sparsemap chunkSize * tileSize exceeds int range.");

    const std::size_t pixelsPerLayer = static_cast<std::size_t>(tileSize.x) * static_cast<std::size_t>(tileSize.y);

    mCacheTemplate.setPallete(palette);
    for (std::size_t i = 0; i < tileGraphics.size(); ++i)
    {
        debugCheck(tileGraphics[i].size() == pixelsPerLayer,
                   "Sparsemap tile graphic layer size must equal tileSize.x * tileSize.y.");
        mCacheTemplate.setPixels(std::span<const std::uint8_t>(tileGraphics[i]), i);
    }
    // No `setMap` — the template is atlas + palette only. Slot clones in
    // ExplorerManager::buildPoolSlots pass `chunkSize` to the override-map constructor.
}

void Sparsemap::addChunk(glm::ivec2 chunkPos, std::span<const std::uint8_t> cellData)
{
    const std::size_t cellsPerChunk =
        static_cast<std::size_t>(mChunkSize.x) * static_cast<std::size_t>(mChunkSize.y);

    if (!cellData.empty())
    {
        debugCheck(cellData.size() == cellsPerChunk,
                   "Sparsemap::addChunk cellData size must equal chunkSize.x * chunkSize.y (or be empty for zero-init).");
    }

    auto [it, inserted] = mChunks.try_emplace(chunkPos);
    ChunkEntry& entry = it->second;
    if (inserted)
        entry.cells.assign(cellsPerChunk, static_cast<std::uint8_t>(0));

    if (!cellData.empty())
        std::copy(cellData.begin(), cellData.end(), entry.cells.begin());

    ++entry.generation;
}

void Sparsemap::removeChunk(glm::ivec2 chunkPos)
{
    mChunks.erase(chunkPos);
}

void Sparsemap::setCell(glm::ivec2 worldCell, std::uint8_t layerIndex)
{
    debugCheck(static_cast<std::size_t>(layerIndex) < mCacheTemplate.layers(),
               "Sparsemap::setCell layer index out of range of registered tile graphics.");

    const glm::ivec2 chunkPos{
        // Floor-division for negative coords too.
        worldCell.x >= 0 ? worldCell.x / mChunkSize.x : -((-worldCell.x + mChunkSize.x - 1) / mChunkSize.x),
        worldCell.y >= 0 ? worldCell.y / mChunkSize.y : -((-worldCell.y + mChunkSize.y - 1) / mChunkSize.y)
    };
    const glm::ivec2 localCell{
        worldCell.x - chunkPos.x * mChunkSize.x,
        worldCell.y - chunkPos.y * mChunkSize.y
    };

    auto [it, inserted] = mChunks.try_emplace(chunkPos);
    ChunkEntry& entry = it->second;
    if (inserted)
    {
        const std::size_t cellsPerChunk =
            static_cast<std::size_t>(mChunkSize.x) * static_cast<std::size_t>(mChunkSize.y);
        entry.cells.assign(cellsPerChunk, static_cast<std::uint8_t>(0));
    }

    const std::size_t localIdx =
        static_cast<std::size_t>(localCell.y) * static_cast<std::size_t>(mChunkSize.x) +
        static_cast<std::size_t>(localCell.x);
    entry.cells[localIdx] = layerIndex;
    ++entry.generation;
}

std::uint8_t Sparsemap::cell(glm::ivec2 worldCell) const
{
    const glm::ivec2 chunkPos{
        worldCell.x >= 0 ? worldCell.x / mChunkSize.x : -((-worldCell.x + mChunkSize.x - 1) / mChunkSize.x),
        worldCell.y >= 0 ? worldCell.y / mChunkSize.y : -((-worldCell.y + mChunkSize.y - 1) / mChunkSize.y)
    };

    auto it = mChunks.find(chunkPos);
    if (it == mChunks.end()) return 0;

    const glm::ivec2 localCell{
        worldCell.x - chunkPos.x * mChunkSize.x,
        worldCell.y - chunkPos.y * mChunkSize.y
    };
    const std::size_t localIdx =
        static_cast<std::size_t>(localCell.y) * static_cast<std::size_t>(mChunkSize.x) +
        static_cast<std::size_t>(localCell.x);
    return it->second.cells[localIdx];
}

void Sparsemap::chunkDataInto(glm::ivec2 chunkPos, std::span<std::uint8_t> out) const
{
    const std::size_t cellsPerChunk =
        static_cast<std::size_t>(mChunkSize.x) * static_cast<std::size_t>(mChunkSize.y);
    debugCheck(out.size() == cellsPerChunk,
               "Sparsemap::chunkDataInto output span size must equal chunkSize.x * chunkSize.y.");

    auto it = mChunks.find(chunkPos);
    if (it == mChunks.end())
    {
        std::fill(out.begin(), out.end(), static_cast<std::uint8_t>(0));
        return;
    }
    std::copy(it->second.cells.begin(), it->second.cells.end(), out.begin());
}

std::uint64_t Sparsemap::chunkGeneration(glm::ivec2 chunkPos) const
{
    auto it = mChunks.find(chunkPos);
    if (it == mChunks.end()) return 0;
    return it->second.generation;
}

}
