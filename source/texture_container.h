#pragma once

#include "texture.h"
#include "texture_mode.h"
#include "indexed_container.h"
#include "dtexture.h"
#include <optional>
#include <glm/glm.hpp>

namespace Nothofagus
{

struct TexturePack
{
    // nullopt for GPU-proxy entries (render target color attachments).
    // CPU-owned textures (IndirectTexture / DirectTexture / TileMapTexture) always have a value.
    std::optional<Texture> texture;
    std::optional<DTexture> dtextureOpt;
    std::optional<DTexture> dpaletteTextureOpt; ///< GPU palette texture (only for indirect textures).
    std::optional<DTexture> dmapTextureOpt;     ///< GPU map texture (only for tile-map textures).
    glm::ivec2 mTextureSize{0, 0}; ///< Cached size — set at creation for both CPU and proxy entries.
    TextureSampleMode minFilter = TextureSampleMode::Nearest;
    TextureSampleMode magFilter = TextureSampleMode::Nearest;
    TextureMode mode = TextureMode::Direct; ///< CPU-side texture kind. Proxy entries (no CPU texture) keep Direct since they are RGBA color attachments.

    bool isProxy() const { return not texture.has_value(); }
    bool isDirty() const { return not dtextureOpt.has_value(); }

    // GPU cleanup is done externally via the backend before calling clear().
    void clear()
    {
        dtextureOpt        = std::nullopt;
        dpaletteTextureOpt = std::nullopt;
        dmapTextureOpt     = std::nullopt;
    }
};

using TextureContainer = IndexedContainer<TexturePack>;

}
