#pragma once

#include "bellota.h"
#include "mesh.h"
#include "dmesh.h"
#include "dmesh3d.h"
#include "tint.h"
#include "indexed_container.h"
#include <optional>

namespace Nothofagus
{

/**
 * @struct BellotaPack
 * @brief Represents a Bellota object along with optional mesh and tint information.
 * 
 * This structure is used to store a Bellota along with its optional mesh data, DMesh, and tint.
 */
struct BellotaPack
{
    Bellota bellota;
    std::optional<Mesh>    meshOpt;
    std::optional<DMesh>   dmeshOpt;
    std::optional<Tint>    tintOpt;
    std::optional<DMesh3D> dmesh3dOpt; ///< GPU mesh for world-space (3D billboard) bellotas.

    bool isDirty() const
    {
        if (bellota.isWorldSpace())
            return not dmesh3dOpt.has_value();
        return not (meshOpt.has_value() and dmeshOpt.has_value());
    }

    void clearMesh()
    {
        meshOpt.reset();
        if (dmeshOpt.has_value())
        {
            DMesh& dmesh = dmeshOpt.value();
            dmesh.clear();
            dmeshOpt.reset();
        }
        if (dmesh3dOpt.has_value())
        {
            dmesh3dOpt.value().clear();
            dmesh3dOpt.reset();
        }
    }

    void clear()
    {
        clearMesh();
        tintOpt.reset();
    }
};

/**
 * @typedef BellotaContainer
 * @brief Container for BellotaPack objects, indexed by an ID.
 * 
 * This type is used to store and manage Bellota objects along with their associated optional data (Mesh, DMesh, Tint).
 */
using BellotaContainer = IndexedContainer<BellotaPack>;

}