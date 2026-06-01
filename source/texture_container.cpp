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

std::uint64_t TexturePack::ensureFlatRep(ActiveBackend& backend)
{
    if (not dflatTextureOpt.has_value())
    {
        if (not texture.has_value())
            return 0;  // proxy / GPU-only texture has no CPU pixels to flatten

        // Static path: CPU-flatten (palette resolved for indirect) and take layer 0.
        // Dynamic (animated/tile-map) sources are routed elsewhere by the caller.
        TextureData data = std::visit(GenerateTextureDataVisitor{}, texture.value());
        const int width  = static_cast<int>(data.width());
        const int height = static_cast<int>(data.height());
        std::span<std::uint8_t> full = data.getDataSpan();
        const std::size_t layerBytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;
        std::span<const std::uint8_t> layer0(full.data(), std::min(layerBytes, full.size()));

        dflatTextureOpt = backend.uploadFlatTexture(layer0, width, height, minFilter, magFilter);
    }
    return backend.imguiHandleOf(*dflatTextureOpt);
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
