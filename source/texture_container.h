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
};

using TextureContainer = IndexedContainer<TexturePack>;

}