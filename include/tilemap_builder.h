#pragma once

#include "tilemap_id.h"
#include "tilemap_explorer_id.h"
#include "texture.h"
#include <glm/glm.hpp>
#include <cstdint>
#include <span>
#include <vector>

namespace Nothofagus
{

class Canvas;

/// Handles returned by `createTilemap` — caller keeps both the Tilemap id (for
/// world-cell edits via `canvas.tilemap(tilemapId).setCell(...)`) and the
/// TilemapExplorer id (for camera/scroll via `canvas.tilemapExplorer(explorerId).setCamera(...)`).
struct TilemapHandles
{
    TilemapId     tilemapId;
    TilemapExplorerId explorerId;
};

/// Build a Tilemap + a TilemapExplorer in one shot and register both with the canvas.
///
/// @param mapSize       World extent in cells (e.g. {256, 256}).
/// @param chunkSize     Cells per pool slot (e.g. {32, 32}). Pool slot count depends
///                      on this and on the canvas's screen size.
/// @param tileSize      Pixel size of one cell.
/// @param palette       Shared palette for all tiles.
/// @param tileGraphics  One entry per atlas layer; each entry is `tileSize.x * tileSize.y`
///                      palette indices.
TilemapHandles createTilemap(Canvas& canvas,
                             glm::ivec2 mapSize,
                             glm::ivec2 chunkSize,
                             glm::ivec2 tileSize,
                             const ColorPallete& palette,
                             std::span<const std::vector<std::uint8_t>> tileGraphics);

}
