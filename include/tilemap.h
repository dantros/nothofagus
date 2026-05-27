#pragma once

#include "texture.h"
#include "tilemap_id.h"
#include <glm/glm.hpp>
#include <cstdint>
#include <span>
#include <vector>

namespace Nothofagus
{

/// World data for huge tilemaps: cell grid, shared tile atlas, palette. Does not render —
/// pair with a `TilemapView` for that. See CLAUDE.md "Huge tilemaps" section for the full design.
class Tilemap
{
public:
    /// @param mapSize       World extent in cells.
    /// @param chunkSize     Cells per pool slot. Pool draw cost scales with chunkSize.
    /// @param tileSize      Pixel size of one cell (one tile graphic).
    /// @param palette       Shared palette for all tiles.
    /// @param tileGraphics  One entry per atlas layer; each entry is `tileSize.x * tileSize.y` palette indices.
    Tilemap(glm::ivec2 mapSize,
            glm::ivec2 chunkSize,
            glm::ivec2 tileSize,
            const ColorPallete& palette,
            std::span<const std::vector<std::uint8_t>> tileGraphics);

    /// Set the tile layer at a world-cell coordinate. Bumps the owning chunk's generation counter.
    void setCell(glm::ivec2 worldCell, std::uint8_t layerIndex);

    /// Read the tile layer at a world-cell coordinate.
    std::uint8_t cell(glm::ivec2 worldCell) const;

    /// True iff `worldCell` lies inside `[0, mapSize.x) × [0, mapSize.y)`.
    bool inBounds(glm::ivec2 worldCell) const
    {
        return worldCell.x >= 0 && worldCell.x < mMapSize.x
            && worldCell.y >= 0 && worldCell.y < mMapSize.y;
    }

    glm::ivec2 mapSize()       const { return mMapSize; }
    glm::ivec2 chunkSize()     const { return mChunkSize; }
    glm::ivec2 tileSize()      const { return mTileSize; }
    glm::ivec2 chunkGridSize() const { return mChunkGridSize; }

    const ColorPallete& palette() const { return mPalette; }

    /// Read-only view of the per-layer tile graphics (one entry per atlas layer).
    std::span<const std::vector<std::uint8_t>> tileGraphics() const
    {
        return std::span<const std::vector<std::uint8_t>>(mTileGraphics.data(), mTileGraphics.size());
    }

    /// Materialize one chunk's cell grid (row-major, `chunkSize.x * chunkSize.y` bytes).
    /// Edge chunks (when mapSize is not divisible by chunkSize) zero-fill the out-of-world cells.
    /// Used by `TilemapView` slots when scrolling brings a chunk into view.
    std::vector<std::uint8_t> chunkData(glm::ivec2 chunkPos) const;

    /// Same as `chunkData`, but writes into a caller-provided buffer — no allocation.
    /// `out.size()` must equal `chunkSize.x * chunkSize.y`. Used on the per-frame
    /// re-sync hot path by `TilemapView`.
    void chunkDataInto(glm::ivec2 chunkPos, std::span<std::uint8_t> out) const;

    /// Generation counter for one chunk — bumps on any `setCell` inside that chunk.
    /// `TilemapView` slots compare against their `syncedGeneration` to detect world edits.
    std::uint64_t chunkGeneration(glm::ivec2 chunkPos) const;

private:
    glm::ivec2 mMapSize;
    glm::ivec2 mChunkSize;
    glm::ivec2 mTileSize;
    glm::ivec2 mChunkGridSize;
    ColorPallete mPalette;
    std::vector<std::vector<std::uint8_t>> mTileGraphics;
    std::vector<std::uint8_t> mCellGrid;        ///< Row-major, length `mapSize.x * mapSize.y`.
    std::vector<std::uint64_t> mChunkGenerations; ///< Length `chunkGridSize.x * chunkGridSize.y`.
};

}
