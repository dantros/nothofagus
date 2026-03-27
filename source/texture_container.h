#pragma once

#include "texture.h"
#include "indexed_container.h"
#include "dtexture.h"
#include <optional>
#include <glm/glm.hpp>

namespace Nothofagus
{

struct TexturePack
{
    // nullopt for GPU-proxy entries (render target color attachments).
    // CPU-owned textures (IndirectTexture / DirectTexture) always have a value.
    std::optional<Texture> texture;
    std::optional<DTexture> dtextureOpt;
    glm::ivec2 mTextureSize{0, 0}; ///< Cached size — set at creation for both CPU and proxy entries.
    TextureSampleMode minFilter = TextureSampleMode::Nearest;
    TextureSampleMode magFilter = TextureSampleMode::Nearest;

    bool isProxy() const { return not texture.has_value(); }
    bool isDirty() const { return not dtextureOpt.has_value(); }

    void clear()
    {
        if (dtextureOpt.has_value())
        {
            // Proxy entries borrow the GL handle from DRenderTarget — do not delete it here.
            if (not isProxy())
                dtextureOpt.value().clear();
            dtextureOpt = std::nullopt;
        }
    }
};

using TextureContainer = IndexedContainer<TexturePack>;

}