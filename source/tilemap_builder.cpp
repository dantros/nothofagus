#include "tilemap_builder.h"
#include "tilemap.h"
#include "tilemap_explorer.h"
#include "canvas.h"

namespace Nothofagus
{

TilemapHandles createTilemap(Canvas& canvas,
                             glm::ivec2 mapSize,
                             glm::ivec2 chunkSize,
                             glm::ivec2 tileSize,
                             const ColorPallete& palette,
                             std::span<const std::vector<std::uint8_t>> tileGraphics)
{
    TilemapId tilemapId = canvas.addTilemap(
        Tilemap(mapSize, chunkSize, tileSize, palette, tileGraphics));
    TilemapExplorerId viewId = canvas.addTilemapExplorer(TilemapExplorer(tilemapId));
    return TilemapHandles{ tilemapId, viewId };
}

}
