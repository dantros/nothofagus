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
    // rep) while it's shown; stamp this frame so endFrame() keeps it. The unpin
    // happens in endFrame() once it stops being requested.
    if (mPinned.find(textureId.id) == mPinned.end())
        mAssets.pinTexture(textureId);
    mPinned[textureId.id] = mFrameCounter;

    return pack.ensureFlatRep(mBackend);
}

glm::ivec2 ImguiImageManager::size(TextureId textureId) const
{
    const TextureContainer& textures = mAssets.textures();
    if (not textures.contains(textureId.id))
        return glm::ivec2(0, 0);
    return textures.at(textureId.id).mTextureSize;
}

void ImguiImageManager::endFrame()
{
    // Unpin sources not requested this frame; once unpinned (and unreferenced by
    // any real bellota) the next clearUnusedTextures frees the source + flat rep.
    for (auto it = mPinned.begin(); it != mPinned.end();)
    {
        if (it->second != mFrameCounter)
        {
            mAssets.unpinTexture(TextureId{it->first});
            it = mPinned.erase(it);
        }
        else
        {
            ++it;
        }
    }
    ++mFrameCounter;
}

}
