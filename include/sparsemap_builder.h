#pragma once

#include "sparsemap_id.h"
#include "sparsemap_explorer_id.h"
#include "texture.h"
#include <glm/glm.hpp>
#include <cstdint>
#include <span>
#include <vector>

namespace Nothofagus
{

class Canvas;

/// Handles returned by `createSparsemap` — caller keeps both the Sparsemap id (for
/// chunk lifecycle via `canvas.sparsemap(sparsemapId).addChunk(...)` /
/// `setCell(...)`) and the SparsemapExplorer id (for camera/scroll via
/// `canvas.sparsemapExplorer(explorerId).setCamera(...)`).
struct SparsemapHandles
{
    SparsemapId         sparsemapId;
    SparsemapExplorerId explorerId;
};

/// Build a Sparsemap + a SparsemapExplorer in one shot and register both with the canvas.
/// The world starts empty — call `addChunk` / `setCell` afterwards to populate it.
///
/// @param chunkSize     Cells per pool slot (e.g. {32, 32}).
/// @param tileSize      Pixel size of one cell.
/// @param palette       Shared palette for all tiles.
/// @param tileGraphics  One entry per atlas layer; each entry is `tileSize.x * tileSize.y`
///                      palette indices.
SparsemapHandles createSparsemap(Canvas& canvas,
                                 glm::ivec2 chunkSize,
                                 glm::ivec2 tileSize,
                                 const ColorPallete& palette,
                                 std::span<const std::vector<std::uint8_t>> tileGraphics);

}
