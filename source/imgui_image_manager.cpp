#include "imgui_image_manager.h"

#include "texture.h"  // GenerateTextureDataVisitor, GetTextureSizeVisitor, TextureData
#include <spdlog/spdlog.h>
#include <algorithm>
#include <variant>

namespace Nothofagus
{

namespace
{

// CPU-flattenable sources (phases 1 + 2): a DirectTexture, or a plain
// single-layer IndirectTexture (no animation layers, no tile-map cell grid).
// These resolve to a single RGBA frame via generateTextureData(). Animated
// (multi-layer) and tile-map indirect textures are dynamic / cell-composed and
// need the render-target path (phase 3); flattening their layer 0 would show a
// single tile graphic rather than the intended image, so they are declined here.
bool isCpuFlattenable(const Texture& texture)
{
    if (std::holds_alternative<DirectTexture>(texture))
        return true;
    const IndirectTexture& indirect = std::get<IndirectTexture>(texture);
    return indirect.layers() == 1 && not indirect.hasMap();
}

}  // namespace

std::uint64_t ImguiImageManager::handle(TextureId textureId)
{
    if (auto cached = mCache.find(textureId.id); cached != mCache.end())
        return cached->second.handle;

    if (not mTextures.contains(textureId.id))
        return 0;

    const TexturePack& pack = mTextures.at(textureId.id);
    if (not pack.texture.has_value())
        return 0;  // render-target proxy / GPU-only texture has no CPU pixels to flatten

    const Texture& cpuTexture = pack.texture.value();

    if (not isCpuFlattenable(cpuTexture))
    {
        // Cache the decision (handle 0) so we classify + warn once, not per frame.
        const glm::ivec2 fullSize = std::visit(GetTextureSizeVisitor{}, cpuTexture);
        spdlog::warn("ImguiImageManager: animated / tile-map texture {} cannot be "
                     "drawn as an inline image yet (render-target path is a later phase) — skipped.",
                     textureId.id);
        mCache[textureId.id] = CachedImage{0, fullSize};
        return 0;
    }

    // Flatten to RGBA8 on the CPU. For an IndirectTexture this resolves the
    // palette (the "conversion" cost); for a DirectTexture it returns the bytes
    // verbatim. Single-layer sources produce exactly one width*height frame.
    TextureData data = std::visit(GenerateTextureDataVisitor{}, cpuTexture);
    const int width  = static_cast<int>(data.width());
    const int height = static_cast<int>(data.height());

    std::span<std::uint8_t> full = data.getDataSpan();
    const std::size_t layerBytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;
    std::span<const std::uint8_t> layer0(full.data(), std::min(layerBytes, full.size()));

    const std::uint64_t newHandle =
        mBackend.createImguiImage2D(layer0, width, height, pack.minFilter, pack.magFilter);
    mCache[textureId.id] = CachedImage{newHandle, glm::ivec2(width, height)};
    return newHandle;
}

glm::ivec2 ImguiImageManager::size(TextureId textureId) const
{
    if (auto cached = mCache.find(textureId.id); cached != mCache.end())
        return cached->second.size;
    return glm::ivec2(0, 0);
}

}
