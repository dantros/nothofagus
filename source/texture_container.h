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
};

using TextureContainer = IndexedContainer<TexturePack>;

}