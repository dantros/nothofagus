#include "texture_container.h"

#include <span>
#include <cstdint>
#include <algorithm>
#include <variant>

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
    // Flat (ImGui) rep is a normal texture in the backend's map.
    if (dflatTextureOpt.has_value())
        backend.freeTexture(*dflatTextureOpt);
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
        dtextureOpt = backend.uploadTexture(texture.value(), TextureUploadMode::Array, minFilter, magFilter);

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
            dtextureOpt = backend.uploadTexture(texture.value(), TextureUploadMode::Array, minFilter, magFilter);
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

    // Flat (ImGui-bindable) rep — lazily brought online once an ImGui consumer
    // requested it (imguiImageHandle). The Flat upload palette-resolves and takes
    // layer 0 internally. Created the frame after the request, so the handle is
    // ready on the next imguiImageHandle() call (one-frame deferral).
    if (mFlatRequested && not dflatTextureOpt.has_value() && not isProxy() && texture.has_value())
        dflatTextureOpt = backend.uploadTexture(texture.value(), TextureUploadMode::Flat, minFilter, magFilter);
}

}
