#pragma once

#include "texture.h"
#include "indexed_container.h"
#include "dtexture.h"
#include <optional>

namespace Nothofagus
{

struct TexturePack
{
	Texture texture;
	std::optional<DTexture> dtextureOpt;

	bool isDirty() const { return not dtextureOpt.has_value(); }

	void clear()
    {
        if (dtextureOpt.has_value())
        {
            DTexture& dtexture = dtextureOpt.value();
            dtexture.clear();
            dtextureOpt = std::nullopt;
        }
    }
};

using TextureContainer = IndexedContainer<TexturePack>;

}