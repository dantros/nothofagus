#pragma once

#include "bellota.h"
#include "mesh.h"
#include "dmesh.h"
// #include "dmesh3D.h"
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
    std::optional<Mesh> meshOpt;
    std::optional<DMesh> dmeshOpt;
    std::optional<Tint> tintOpt;
};

/**
 * @typedef BellotaContainer
 * @brief Container for BellotaPack objects, indexed by an ID.
 * 
 * This type is used to store and manage Bellota objects along with their associated optional data (Mesh, DMesh, Tint).
 */
using BellotaContainer = IndexedContainer<BellotaPack>;

}