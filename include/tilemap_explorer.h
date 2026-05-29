#pragma once

#include "tilemap_id.h"
#include "tilemap_explorer_id.h"
#include <glm/glm.hpp>
#include <cstdint>

namespace Nothofagus
{

/// Windowed renderer for a `Tilemap`: a canvas-owned chunk pool draws the visible region,
/// scrolling via `setCamera`. Camera = world-pixel coordinate shown at the canvas center;
/// `(0,0)` centers the world origin. See CLAUDE.md "Huge tilemaps" for pool semantics and
/// explorer-managed lifecycle rules.
class TilemapExplorer
{
public:
    explicit TilemapExplorer(TilemapId tilemapId):
        mTilemapId(tilemapId),
        mCamera(0.0f, 0.0f),
        mDepthOffset(0)
    {}

    TilemapId tilemap() const { return mTilemapId; }

    void      setCamera(glm::vec2 worldOffset) { mCamera = worldOffset; }
    glm::vec2 camera() const { return mCamera; }

    void          setDepthOffset(std::int8_t offset) { mDepthOffset = offset; }
    std::int8_t   depthOffset() const { return mDepthOffset; }

private:
    TilemapId   mTilemapId;
    glm::vec2   mCamera;
    std::int8_t mDepthOffset;
};

}
