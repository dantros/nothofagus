#include "sparse_land.h"
#include "explorer.h"
#include "check.h"
#include <algorithm>
#include <cstdint>
#include <limits>

namespace Nothofagus
{

// Concept conformance: if a `LandType` requirement breaks the build at this line,
// the missing/changed method shows up clearly instead of as a generic template error.
static_assert(LandType<SparseLand>);

namespace
{

struct DivMod { int quotient; int remainder; };

/// Floor-division with always-non-negative remainder in `[0, d)`. Used to map a
/// world-cell coordinate (possibly negative) onto its owning chunk coord and
/// intra-chunk index in one step. Overflow-safe at `INT_MIN`: there is no
/// negation of `n`, and `d > 0` is guaranteed by the `SparseLand` ctor's chunkSize
/// assertion, so `n / d` is well-defined for every representable `n` (the only UB
/// case for built-in `/` is `INT_MIN / -1`, which `d > 0` rules out). The product
/// `q * d` cannot overflow either, since truncating division yields `|q * d| <= |n|`.
constexpr DivMod floorDivMod(int n, int d)
{
    int q = n / d;
    int r = n - q * d;
    if (r < 0) { --q; r += d; }
    return {q, r};
}

}  // namespace

SparseLand::SparseLand(glm::ivec2 chunkSize,
                     glm::ivec2 tileSize,
                     const ColorPallete& palette,
                     std::span<const std::vector<std::uint8_t>> tileGraphics):
    // Clear color is required by the IndirectTexture ctor but unused here —
    // the template never renders directly; only pool-slot clones reach the GPU.
    mCacheTemplate(tileSize, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f), tileGraphics.size()),
    mChunkSize(chunkSize)
{
    debugCheck(chunkSize.x > 0 && chunkSize.y > 0, "SparseLand chunkSize must be positive.");
    debugCheck(tileSize.x > 0 && tileSize.y > 0, "SparseLand tileSize must be positive.");
    debugCheck(!tileGraphics.empty(), "SparseLand requires at least one tile graphic layer.");

    // Mirrors the one-time bound from `DenseLand`: with chunkSize * tileSize fitting in int,
    // the per-frame chunk-pixel math in explorer_manager.cpp can stay in plain int.
    debugCheck(
        static_cast<std::int64_t>(chunkSize.x) * static_cast<std::int64_t>(tileSize.x) <= std::numeric_limits<int>::max()
     && static_cast<std::int64_t>(chunkSize.y) * static_cast<std::int64_t>(tileSize.y) <= std::numeric_limits<int>::max(),
        "SparseLand chunkSize * tileSize exceeds int range.");

    const std::size_t pixelsPerLayer = static_cast<std::size_t>(tileSize.x) * static_cast<std::size_t>(tileSize.y);

    mCacheTemplate.setPallete(palette);
    for (std::size_t i = 0; i < tileGraphics.size(); ++i)
    {
        debugCheck(tileGraphics[i].size() == pixelsPerLayer,
                   "SparseLand tile graphic layer size must equal tileSize.x * tileSize.y.");
        mCacheTemplate.setPixels(std::span<const std::uint8_t>(tileGraphics[i]), i);
    }
    // No `setMap` — the template is atlas + palette only. Slot clones in
    // ExplorerManager::buildPoolSlots pass `chunkSize` to the override-map constructor.
}

void SparseLand::addChunk(glm::ivec2 chunkPos, std::span<const std::uint8_t> cellData)
{
    const std::size_t cellsPerChunk =
        static_cast<std::size_t>(mChunkSize.x) * static_cast<std::size_t>(mChunkSize.y);

    if (!cellData.empty())
    {
        debugCheck(cellData.size() == cellsPerChunk,
                   "SparseLand::addChunk cellData size must equal chunkSize.x * chunkSize.y (or be empty for zero-init).");
    }

    auto [it, inserted] = mChunks.try_emplace(chunkPos);
    ChunkEntry& entry = it->second;
    if (inserted)
        entry.cells.assign(cellsPerChunk, static_cast<std::uint8_t>(0));

    if (!cellData.empty())
        std::copy(cellData.begin(), cellData.end(), entry.cells.begin());

    ++entry.generation;
}

void SparseLand::removeChunk(glm::ivec2 chunkPos)
{
    mChunks.erase(chunkPos);
}

void SparseLand::setCell(glm::ivec2 worldCell, std::uint8_t layerIndex)
{
    debugCheck(static_cast<std::size_t>(layerIndex) < mCacheTemplate.layers(),
               "SparseLand::setCell layer index out of range of registered tile graphics.");

    const auto [chunkX, localX] = floorDivMod(worldCell.x, mChunkSize.x);
    const auto [chunkY, localY] = floorDivMod(worldCell.y, mChunkSize.y);
    const glm::ivec2 chunkPos{chunkX, chunkY};

    auto [it, inserted] = mChunks.try_emplace(chunkPos);
    ChunkEntry& entry = it->second;
    if (inserted)
    {
        const std::size_t cellsPerChunk =
            static_cast<std::size_t>(mChunkSize.x) * static_cast<std::size_t>(mChunkSize.y);
        entry.cells.assign(cellsPerChunk, static_cast<std::uint8_t>(0));
    }

    const std::size_t localIdx =
        static_cast<std::size_t>(localY) * static_cast<std::size_t>(mChunkSize.x) +
        static_cast<std::size_t>(localX);
    entry.cells[localIdx] = layerIndex;
    ++entry.generation;
}

std::uint8_t SparseLand::cell(glm::ivec2 worldCell) const
{
    const auto [chunkX, localX] = floorDivMod(worldCell.x, mChunkSize.x);
    const auto [chunkY, localY] = floorDivMod(worldCell.y, mChunkSize.y);
    const glm::ivec2 chunkPos{chunkX, chunkY};

    auto it = mChunks.find(chunkPos);
    if (it == mChunks.end()) return 0;

    const std::size_t localIdx =
        static_cast<std::size_t>(localY) * static_cast<std::size_t>(mChunkSize.x) +
        static_cast<std::size_t>(localX);
    return it->second.cells[localIdx];
}

bool SparseLand::chunkInBounds(glm::ivec2 chunkPos) const
{
    return mChunks.contains(chunkPos);
}

void SparseLand::chunkDataInto(glm::ivec2 chunkPos, std::span<std::uint8_t> out) const
{
    const std::size_t cellsPerChunk =
        static_cast<std::size_t>(mChunkSize.x) * static_cast<std::size_t>(mChunkSize.y);
    debugCheck(out.size() == cellsPerChunk,
               "SparseLand::chunkDataInto output span size must equal chunkSize.x * chunkSize.y.");

    auto it = mChunks.find(chunkPos);
    if (it == mChunks.end())
    {
        std::fill(out.begin(), out.end(), static_cast<std::uint8_t>(0));
        return;
    }
    std::copy(it->second.cells.begin(), it->second.cells.end(), out.begin());
}

std::uint64_t SparseLand::chunkGeneration(glm::ivec2 chunkPos) const
{
    auto it = mChunks.find(chunkPos);
    if (it == mChunks.end()) return 0;
    return it->second.generation;
}

}
