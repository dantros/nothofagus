#include "texture_container.h"

#include <span>
#include <cstdint>

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

void TexturePack::syncToGpu(ActiveBackend& backend)
{
    const bool isIndirectOrTileMap =
        mode == TextureMode::Indirect ||
        mode == TextureMode::TileMap;

    if (isDirty() && !isProxy())
    {
        // First upload — bring atlas, palette, and (for tilemaps) map online,
        // then wire them together via the backend's link* binding ops.
        dtextureOpt = backend.uploadTexture(texture.value(), minFilter, magFilter);

        if (isIndirectOrTileMap)
        {
            auto& indirectTexture = std::get<IndirectTexture>(texture.value());
            dpaletteTextureOpt = backend.uploadPaletteTexture(
                indirectTexture.generatePaletteData());

            if (mode == TextureMode::TileMap)
            {
                const auto mapData = indirectTexture.generateMapData();
                dmapTextureOpt = backend.uploadTileMapTexture(
                    std::span<const std::uint8_t>(mapData), indirectTexture.mapSize());
                backend.linkTileMapTextures(*dtextureOpt, *dmapTextureOpt, *dpaletteTextureOpt);
            }
            else
            {
                backend.linkIndirectTextures(*dtextureOpt, *dpaletteTextureOpt);
            }
            indirectTexture.clearAtlasDirty();
            indirectTexture.clearMapDirty();
            indirectTexture.clearPaletteDirty();
        }
    }
    else if (isIndirectOrTileMap && !isProxy() && dtextureOpt.has_value())
    {
        // Refresh path — patch atlas / map / palette independently based on
        // the IndirectTexture's per-sub-resource dirty flags.
        auto& indirectTexture = std::get<IndirectTexture>(texture.value());

        if (indirectTexture.isAtlasDirty() && dpaletteTextureOpt.has_value())
        {
            backend.freeTexture(*dtextureOpt);
            dtextureOpt = backend.uploadTexture(texture.value(), minFilter, magFilter);
            if (mode == TextureMode::TileMap && dmapTextureOpt.has_value())
                backend.linkTileMapTextures(*dtextureOpt, *dmapTextureOpt, *dpaletteTextureOpt);
            else
                backend.linkIndirectTextures(*dtextureOpt, *dpaletteTextureOpt);
            indirectTexture.clearAtlasDirty();
        }
        if (mode == TextureMode::TileMap && indirectTexture.isMapDirty()
            && dmapTextureOpt.has_value() && dpaletteTextureOpt.has_value())
        {
            backend.freeTileMapTexture(*dmapTextureOpt);
            const auto mapData = indirectTexture.generateMapData();
            dmapTextureOpt = backend.uploadTileMapTexture(
                std::span<const std::uint8_t>(mapData), indirectTexture.mapSize());
            backend.linkTileMapTextures(*dtextureOpt, *dmapTextureOpt, *dpaletteTextureOpt);
            indirectTexture.clearMapDirty();
        }
        if (indirectTexture.isPaletteDirty() && dpaletteTextureOpt.has_value())
        {
            backend.updatePaletteTexture(*dpaletteTextureOpt, indirectTexture.generatePaletteData());
            indirectTexture.clearPaletteDirty();
        }
    }
}

}
