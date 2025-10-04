#pragma once

#include "bellota.h"
#include "mesh.h"
#include "texture_container.h"

namespace Nothofagus
{

Mesh generateMesh2(const glm::ivec2& size);
Mesh generateMesh(const TextureContainer& textures, const Bellota& bellota);

}