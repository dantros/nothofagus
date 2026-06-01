#include "imgui_image_manager.h"

#include "asset_registry.h"
#include "texture.h"  // DirectTexture / IndirectTexture
#include <spdlog/spdlog.h>
#include <variant>

namespace Nothofagus
{

namespace
{

// Static (CPU-flattenable) sources: a DirectTexture, or a plain single-layer
// IndirectTexture (no animation layers, no tile-map cell grid). Animated /
// tile-map sources are dynamic and need the render-target flat rep (Phase 3).
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
    TextureContainer& textures = mAssets.textures();
    if (not textures.contains(textureId.id))
        return 0;

    TexturePack& pack = textures.at(textureId.id);
    if (not pack.texture.has_value())
        return 0;  // render-target proxy / GPU-only texture has no CPU pixels

    if (not isCpuFlattenable(pack.texture.value()))
    {
        if (mDeclinedWarned.insert(textureId.id).second)
            spdlog::warn("ImguiImageManager: animated / tile-map texture {} cannot be drawn as an "
                         "inline image yet (dynamic render-target path is a later phase) — skipped.",
                         textureId.id);
        return 0;
    }

    // Pin the source so the per-frame texture GC doesn't drop it (and its flat
    // rep) while it's shown. Phase 1 pins permanently; Phase 2 adds the unpin.
    if (mPinned.insert(textureId.id).second)
        mAssets.pinTexture(textureId);

    return pack.ensureFlatRep(mBackend);
}

glm::ivec2 ImguiImageManager::size(TextureId textureId) const
{
    const TextureContainer& textures = mAssets.textures();
    if (not textures.contains(textureId.id))
        return glm::ivec2(0, 0);
    return textures.at(textureId.id).mTextureSize;
}

}
