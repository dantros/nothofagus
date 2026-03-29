#pragma once

#include <vector>

namespace Nothofagus
{

/**
 * @struct Mesh3D
 * @brief CPU-side 3D mesh: interleaved vertex data (x, y, z, u, v) + triangle indices.
 */
struct Mesh3D
{
    std::vector<float>        vertices; ///< Packed floats: x, y, z, u, v per vertex (5 floats each)
    std::vector<unsigned int> indices;  ///< Triangle indices (3 per triangle)
};

} // namespace Nothofagus
