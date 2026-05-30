#include "texture_container.h"

namespace Nothofagus
{

void TexturePack::freeGpuResources(ActiveBackend& backend)
{
    if (dpaletteTextureOpt.has_value())
        backend.freePaletteTexture(*dpaletteTextureOpt);
    if (dmapTextureOpt.has_value())
        backend.freeTileMapTexture(*dmapTextureOpt);
    // Proxy textures' GPU handles are owned by RenderTargetPack and freed via
    // backend.freeRenderTarget; skip them here to avoid a double-free.
    if (dtextureOpt.has_value() && !isProxy())
        backend.freeTexture(*dtextureOpt);
    clear();
}

}
