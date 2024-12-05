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

struct BellotaPack
{
    Bellota bellota;
    std::optional<Mesh> meshOpt;
    std::optional<DMesh> dmeshOpt;
    std::optional<Tint> tintOpt;
};

using BellotaContainer = IndexedContainer<BellotaPack>;

struct AnimatedBellotaPack
{
    AnimatedBellota animatedBellota;
    std::optional<Mesh> meshOpt;
    std::optional<DMesh3D> dmeshOpt;
    std::optional<Tint> tintOpt;
};

using AnimatedBellotaContainer = IndexedContainer<AnimatedBellotaPack>;

}