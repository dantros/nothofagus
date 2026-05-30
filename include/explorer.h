#pragma once

#include "texture.h"
#include <glm/glm.hpp>
#include <concepts>
#include <cstdint>
#include <span>

namespace Nothofagus
{

/// Shared interface satisfied by both `Tilemap` (dense) and `Sparsemap` (sparse). The
/// `Explorer<T>` and the internal `ExplorerManager<T>` depend on this concept; satisfying
/// it is what makes a backend pluggable into the chunk-pool renderer.
template<typename T>
concept TilemapLike = requires(const T& t, glm::ivec2 coord, std::span<std::uint8_t> out) {
    { t.chunkSize()            } -> std::same_as<glm::ivec2>;
    { t.tileSize()             } -> std::same_as<glm::ivec2>;
    { t.chunkPixelSize()       } -> std::same_as<glm::ivec2>;
    { t.palette()              } -> std::same_as<const ColorPallete&>;
    { t.cacheTexture()         } -> std::same_as<const IndirectTexture&>;
    { t.chunkInBounds(coord)   } -> std::same_as<bool>;
    { t.chunkGeneration(coord) } -> std::same_as<std::uint64_t>;
    t.chunkDataInto(coord, out);
};

/// Per-backend ID type binding: maps a `TilemapLike` type to its corresponding land ID
/// (the world the explorer roams) and explorer ID. Specialized for each concrete backend
/// alongside its type definition (see `tilemap.h` and `sparsemap.h`).
template<typename T> struct TilemapTraits;

/// Windowed renderer handle for any `TilemapLike` backend: holds the camera (world-pixel
/// coordinate shown at the canvas center) and the depth offset for the pool's bellotas.
/// The actual pool of bellotas + `IndirectTexture` slots is owned by `ExplorerManager<T>`
/// inside the canvas. See CLAUDE.md "Huge tilemaps" / "Sparse tilemaps" for pool semantics
/// and explorer-managed lifecycle rules.
template<TilemapLike T>
class Explorer
{
public:
    using LandId = typename TilemapTraits<T>::LandId;

    explicit Explorer(LandId landId):
        mLandId(landId),
        mCamera(0.0f, 0.0f),
        mDepthOffset(0)
    {}

    LandId land() const { return mLandId; }

    void      setCamera(glm::vec2 worldOffset) { mCamera = worldOffset; }
    glm::vec2 camera() const { return mCamera; }

    void          setDepthOffset(std::int8_t offset) { mDepthOffset = offset; }
    std::int8_t   depthOffset() const { return mDepthOffset; }

private:
    LandId      mLandId;
    glm::vec2   mCamera;
    std::int8_t mDepthOffset;
};

}
