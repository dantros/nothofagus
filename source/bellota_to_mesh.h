#pragma once

#include "bellota.h"
#include "mesh.h"
#include "texture_container.h"

namespace Nothofagus
{

Mesh generateMesh(const TextureContainer& textures, const Bellota& bellota);
Mesh generateMesh(const TextureArrayContainer& textures, const AnimatedBellota& bellota);

}