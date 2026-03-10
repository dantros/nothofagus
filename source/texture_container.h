#pragma once

#include "texture.h"
#include "indexed_container.h"
#include "dtexture.h"
#include <optional>

namespace Nothofagus
{

struct TexturePack
{
    // nullopt for GPU-proxy entries (render target color attachments).
    // CPU-owned textures (IndirectTexture / DirectTexture) always have a value.
    std::optional<Texture> texture;
    std::optional<DTexture> dtextureOpt;

    bool isProxy() const { return not texture.has_value(); }
    bool isDirty() const { return not dtextureOpt.has_value(); }

    void clear()
    {
        if (dtextureOpt.has_value())
        {
            // Proxy entries borrow the GL handle from DFramebuffer — do not delete it here.
            if (not isProxy())
                dtextureOpt.value().clear();
            dtextureOpt = std::nullopt;
        }
    }
};

using TextureContainer = IndexedContainer<TexturePack>;

}