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
    // rep) while it's shown: an invisible bellota referencing it. Stamp this
    // frame so endFrame() keeps it; the bellota is removed once it stops being
    // requested. (This pays the standard bellota cost — an auto-quad mesh that's
    // never drawn — which a later step can make cheaper.)
    auto pinned = mPinned.find(textureId.id);
    if (pinned == mPinned.end())
    {
        const BellotaId pinBellota = mAssets.addBellota(Bellota(Transform(), textureId));
        mAssets.bellota(pinBellota).visible() = false;
        pinned = mPinned.emplace(textureId.id, PinnedImage{pinBellota, mFrameCounter}).first;
    }
    pinned->second.lastTouchedFrame = mFrameCounter;

    // Request the flat rep; syncToGpu creates it later this frame, so the handle
    // is ready on the next frame's call (one-frame deferral). Return 0 until then.
    pack.mFlatRequested = true;
    if (pack.dflatTextureOpt.has_value())
        return mBackend.imguiHandleOf(*pack.dflatTextureOpt);
    return 0;
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
    // Remove the pin bellota of any source not requested this frame; once it (and
    // any real bellota) is gone the next clearUnusedTextures frees the source +
    // flat rep.
    for (auto it = mPinned.begin(); it != mPinned.end();)
    {
        if (it->second.lastTouchedFrame != mFrameCounter)
        {
            mAssets.removeBellota(it->second.bellota);
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
