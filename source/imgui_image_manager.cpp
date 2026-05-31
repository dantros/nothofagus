#include "imgui_image_manager.h"

#include "texture.h"  // GenerateTextureDataVisitor, TextureData
#include <algorithm>
#include <variant>

namespace Nothofagus
{

std::uint64_t ImguiImageManager::handle(TextureId textureId)
{
    if (auto cached = mCache.find(textureId.id); cached != mCache.end())
        return cached->second.handle;

    if (not mTextures.contains(textureId.id))
        return 0;

    const TexturePack& pack = mTextures.at(textureId.id);
    if (not pack.texture.has_value())
        return 0;  // render-target proxy / GPU-only texture has no CPU pixels to flatten

    // Flatten to RGBA8 on the CPU. For an IndirectTexture this resolves the
    // palette; for a DirectTexture it returns the bytes verbatim. Multi-layer
    // sources collapse to layer 0 (first animation frame) for now.
    TextureData data = std::visit(GenerateTextureDataVisitor{}, pack.texture.value());
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
