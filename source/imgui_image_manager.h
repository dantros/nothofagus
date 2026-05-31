#pragma once

#include "bellota.h"            // TextureId
#include "texture_container.h"  // TextureContainer
#include "backends/render_backend_select.h"  // ActiveBackend
#include <cstdint>
#include <unordered_map>

namespace Nothofagus
{

/**
 * @brief Bridges engine `TextureId`s to ImGui-bindable image handles
 *        (`ImTextureID`) for `ImGui::Image` — the basis of inline markdown
 *        images.
 *
 * Engine textures live as 2D-array images sampled with a per-draw layer index
 * and (for indirect textures) a separate palette, which ImGui's own shaders
 * cannot sample. This manager flattens a texture to plain RGBA8 on the CPU
 * (via `Texture::generateTextureData`, which resolves palettes) and uploads
 * its first layer into a plain 2D GPU texture through
 * `ActiveBackend::createImguiImage2D`.
 *
 * A handle + its pixel size are cached per `TextureId` on first request and
 * kept for the canvas lifetime — `IndexedContainer` ids are never recycled, so
 * a cached handle never aliases a different texture. Because the upload is a
 * self-contained RGBA snapshot, the source `TextureId` may be released (e.g. by
 * the per-frame texture GC, since a markdown-only image is referenced by no
 * bellota) without affecting the inline image — the cache keeps both the GPU
 * handle and the size. Mutating the source afterwards does not refresh the
 * image (animated / dynamic sources are a later phase). The GPU images are
 * reclaimed by the backend's `shutdown()`.
 */
class ImguiImageManager
{
public:
    ImguiImageManager(ActiveBackend& backend, TextureContainer& textures)
        : mBackend(backend), mTextures(textures)
    {
    }

    /// Resolve a `TextureId` to an ImGui-bindable handle (an `ImTextureID`
    /// value). Returns 0 if the id is unknown or refers to a GPU-only proxy
    /// (render-target) texture with no CPU pixels to flatten.
    std::uint64_t handle(TextureId textureId);

    /// Pixel size of the cached image for `textureId`, or {0, 0} if it has not
    /// been resolved via `handle(...)` yet. Reads only the cache — safe after
    /// the source texture has been released.
    glm::ivec2 size(TextureId textureId) const;

private:
    struct CachedImage
    {
        std::uint64_t handle;
        glm::ivec2    size;
    };

    ActiveBackend&    mBackend;
    TextureContainer& mTextures;

    std::unordered_map<std::size_t, CachedImage> mCache; ///< TextureId.id -> cached image.
};

}
