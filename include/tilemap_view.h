#pragma once

#include "tilemap_id.h"
#include "tilemap_view_id.h"
#include <glm/glm.hpp>
#include <cstdint>

namespace Nothofagus
{

/**
 * @class TilemapView
 * @brief Renders a windowed view of a `Tilemap` via a small pool of
 *        `IndirectTexture` + `Bellota` slots managed internally by the canvas.
 *
 * Pool size is determined at registration based on the canvas's screen size +
 * a one-chunk margin on each side. Slots are anchored to their pool indices;
 * as the camera scrolls, world chunks rotate through the slots — only the
 * border slots crossing into/out of view need their cell data rewritten.
 *
 * Bellotas inside the pool are tagged view-managed: user-side
 * `canvas.removeBellota`/`canvas.removeTexture` calls on those ids will
 * `debugCheck`-fail. Use `canvas.removeTilemapView(id)` to tear the pool down.
 *
 * Camera convention (v1): `camera()` is the world-pixel coordinate that
 * appears at the canvas center. `(0,0)` = world origin centered on screen.
 */
class TilemapView
{
public:
    explicit TilemapView(TilemapId tilemapId):
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
