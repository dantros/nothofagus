#pragma once

#include "../include/world_bellota.h"
#include "dmesh3d.h"
#include "indexed_container.h"
#include <optional>

namespace Nothofagus
{

/**
 * @struct WorldBellotaPack
 * @brief Stores a WorldBellota alongside its GPU mesh.
 *
 * The VBO is allocated once (GL_DYNAMIC_DRAW) and updated every frame with
 * billboard corners computed from the current camera orientation.
 * isDirty() only covers the VAO/VBO/EBO allocation step, not per-frame updates.
 */
struct WorldBellotaPack
{
    WorldBellota           worldBellota;
    std::optional<DMesh3D> dmeshOpt;

    bool isDirty() const
    {
        return not dmeshOpt.has_value();
    }

    void clear()
    {
        if (dmeshOpt.has_value())
        {
            dmeshOpt.value().clear();
            dmeshOpt.reset();
        }
    }
};

using WorldBellotaContainer = IndexedContainer<WorldBellotaPack>;

} // namespace Nothofagus
