#pragma once

#include <unordered_map>
#include <unordered_set>
#include "bellota.h"
#include "mesh.h"

namespace Nothofagus
{

class MeshUsageMonitor
{
public:
    MeshUsageMonitor() = default;

    bool addUnusedMesh(MeshId meshId);

    bool hasUnusedMesh(MeshId meshId) const;

    bool hasMesh(MeshId meshId) const;

    bool removeUnusedMesh(MeshId meshId);

    bool addEntry(BellotaId bellotaId, MeshId meshId);

    bool hasEntry(BellotaId bellotaId, MeshId meshId) const;

    bool removeEntry(BellotaId bellotaId, MeshId meshId);

    const std::unordered_set<MeshId>& getUnusedMeshIds() const;

    void clearUnusedMeshIds();

    std::size_t loadedMeshes() const;

private:
    std::unordered_map<MeshId, std::unordered_set<BellotaId>> mMeshToBellotas;
    std::unordered_set<MeshId> mUnusedMeshes;
};

}
