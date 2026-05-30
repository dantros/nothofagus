#pragma once

#include "texture.h"
#include "sparsemap_id.h"
#include "ivec2_hash.h"
#include <glm/glm.hpp>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace Nothofagus
{

/// World data for sparse tilemaps: a hash-map of chunks at integer chunk coordinates,
/// shared tile atlas, palette. No `mapSize` — the world is unbounded and chunks exist
/// only where added. Pair with a `SparsemapExplorer` for rendering; the same
/// pool-of-bellotas optimization as `Tilemap` applies, just with `chunkInBounds` replacing
/// the dense out-of-world check.
///
/// `setCell` lazy-creates the owning chunk (zero-initialized) if missing — convenient
/// for ad-hoc editing. `addChunk` / `removeChunk` are the bulk streaming paths.
class Sparsemap
{
public:
    /// @param chunkSize     Cells per pool slot.
    /// @param tileSize      Pixel size of one cell (one tile graphic).
    /// @param palette       Shared palette for all tiles.
    /// @param tileGraphics  One entry per atlas layer; each entry is `tileSize.x * tileSize.y` palette indices.
    Sparsemap(glm::ivec2 chunkSize,
              glm::ivec2 tileSize,
              const ColorPallete& palette,
              std::span<const std::vector<std::uint8_t>> tileGraphics);

    /// Insert (or overwrite) a chunk at `chunkPos`. If `cellData` is empty the chunk is
    /// zero-initialized; otherwise its size must equal `chunkSize.x * chunkSize.y`.
    /// Bumps the chunk's generation counter.
    void addChunk(glm::ivec2 chunkPos, std::span<const std::uint8_t> cellData = {});

    /// Erase the chunk at `chunkPos`. No-op if the chunk is not present.
    /// Pool slots displaying it will hide on the next frame via `chunkInBounds`.
    void removeChunk(glm::ivec2 chunkPos);

    /// Write a single cell at world coordinate `worldCell`. Lazy-creates the owning chunk
    /// (zero-initialized) if it is not present. Bumps the owning chunk's generation counter.
    void setCell(glm::ivec2 worldCell, std::uint8_t layerIndex);

    /// Read a single cell. Returns 0 if the owning chunk is not present.
    std::uint8_t cell(glm::ivec2 worldCell) const;

    glm::ivec2 chunkSize()      const { return mChunkSize; }
    glm::ivec2 tileSize()       const { return mCacheTemplate.size(); }
    /// Pixel extent of one chunk: `chunkSize * tileSize`.
    glm::ivec2 chunkPixelSize() const { return mChunkSize * mCacheTemplate.size(); }

    const ColorPallete& palette() const { return mCacheTemplate.pallete(); }

    /// The underlying `IndirectTexture` template (atlas + palette, no `setMap`).
    /// Used by `ExplorerManager` to clone slot textures via the override-map constructor.
    const IndirectTexture& cacheTexture() const { return mCacheTemplate; }

    /// True iff a chunk has been added at `chunkPos`. For sparse maps "in bounds" is
    /// equivalent to "resident in the hash map" — there is no fixed world extent.
    /// Satisfies the `TilemapLike` concept; drives slot visibility in the explorer pool.
    bool chunkInBounds(glm::ivec2 chunkPos) const;

    /// Number of chunks currently resident in the hash-map. Useful for memory accounting / UIs.
    std::size_t chunkCount() const { return mChunks.size(); }

    /// Materialize one chunk's cell grid into `out` (`chunkSize.x * chunkSize.y` bytes,
    /// row-major). Zero-fills if the chunk is not present. Used on the per-frame
    /// re-sync hot path by the explorer.
    void chunkDataInto(glm::ivec2 chunkPos, std::span<std::uint8_t> out) const;

    /// Generation counter for one chunk — bumps on any `setCell` / `addChunk` for that chunk.
    /// Returns 0 if the chunk is not present. `addChunk` and lazy `setCell` both pre-increment
    /// on creation, so a present chunk is always at gen ≥ 1; the chunk-sync pass relies on this
    /// to distinguish "present, freshly added" from "missing" purely by the generation value
    /// (the sentinel matches `PoolSlot::syncedGeneration`'s initial 0). Keep this invariant if
    /// refactoring — starting a fresh chunk at gen=0 would collapse the two states.
    std::uint64_t chunkGeneration(glm::ivec2 chunkPos) const;

private:
    struct ChunkEntry
    {
        std::vector<std::uint8_t> cells;     ///< `chunkSize.x * chunkSize.y` bytes, row-major.
        std::uint64_t             generation{0};
    };

    IndirectTexture mCacheTemplate;          ///< Atlas + palette only; no `setMap`. Cloned for each pool slot.
    glm::ivec2      mChunkSize;
    std::unordered_map<glm::ivec2, ChunkEntry, IVec2Hash> mChunks;
};

}
