#pragma once

#include "bellota.h"
#include "mesh.h"
#include "dmesh.h"
#include "indexed_container.h"
#include <optional>

namespace Nothofagus
{

struct BellotaPack
{
    Bellota bellota;
    std::optional<Mesh> meshOpt;
    std::optional<DMesh> dmeshOpt;
};

using BellotaContainer = IndexedContainer<BellotaPack>;

}