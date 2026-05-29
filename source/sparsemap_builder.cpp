#include "sparsemap_builder.h"
#include "sparsemap.h"
#include "explorer.h"
#include "canvas.h"

namespace Nothofagus
{

SparsemapHandles createSparsemap(Canvas& canvas,
                                 glm::ivec2 chunkSize,
                                 glm::ivec2 tileSize,
                                 const ColorPallete& palette,
                                 std::span<const std::vector<std::uint8_t>> tileGraphics)
{
    SparsemapId sparsemapId = canvas.addSparsemap(
        Sparsemap(chunkSize, tileSize, palette, tileGraphics));
    SparsemapExplorerId explorerId = canvas.addSparsemapExplorer(SparsemapExplorer(sparsemapId));
    return SparsemapHandles{ sparsemapId, explorerId };
}

}
