#pragma once

#include "bellota.h"
#include "tint.h"
#include "indexed_container.h"
#include <optional>

namespace Nothofagus

{

/**
 * @struct BellotaPack
 * @brief Wraps a Bellota with its optional tint.
 *
 * Mesh data (CPU + GPU) lives in MeshContainer / MeshPack, keyed by the
 * Bellota's MeshId — never duplicated here.
 */
struct BellotaPack
{
    Bellota bellota;
    std::optional<Tint> tintOpt;

    void clear()
    {
        tintOpt.reset();
    }
};

/**
 * @typedef BellotaContainer
 * @brief Container for BellotaPack objects, indexed by an ID.
 */
using BellotaContainer = IndexedContainer<BellotaPack>;

}
