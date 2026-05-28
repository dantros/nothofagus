#pragma once

#include "mesh.h"
#include <glm/glm.hpp>

namespace Nothofagus
{

/**
 * @brief Generate the default centered quad mesh for an axis-aligned sprite of the given size.
 *
 * Produces 4 vertices (corners) and 6 indices (two triangles), with UVs spanning
 * the full texture (0..1). Origin is the center of the quad, matching the
 * coordinate convention used elsewhere in the engine.
 */
Mesh generateQuadMesh(const glm::ivec2& size);

}
