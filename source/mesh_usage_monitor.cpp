
#include "mesh_usage_monitor.h"
#include "check.h"

namespace Nothofagus
{

bool MeshUsageMonitor::addUnusedMesh(MeshId meshId)
{
    auto pair = mUnusedMeshes.insert(meshId);
    return pair.second;
}

bool MeshUsageMonitor::hasUnusedMesh(MeshId meshId) const
{
    return mUnusedMeshes.contains(meshId);
}

bool MeshUsageMonitor::hasMesh(MeshId meshId) const
{
    return mUnusedMeshes.contains(meshId) or mMeshToBellotas.contains(meshId);
}

bool MeshUsageMonitor::removeUnusedMesh(MeshId meshId)
{
    if (not mUnusedMeshes.contains(meshId))
        return false;

    const std::size_t elementsErased = mUnusedMeshes.erase(meshId);
    return elementsErased == 1;
}

bool MeshUsageMonitor::addEntry(BellotaId bellotaId, MeshId meshId)
{
    if (mMeshToBellotas.contains(meshId))
    {
        debugCheck(not mUnusedMeshes.contains(meshId));
        std::unordered_set<BellotaId>& bellotaIds = mMeshToBellotas[meshId];
        auto pair = bellotaIds.insert(bellotaId);
        return pair.second;
    }

    debugCheck(mUnusedMeshes.contains(meshId));

    mMeshToBellotas.emplace(meshId, std::unordered_set<BellotaId>{});
    std::unordered_set<BellotaId>& bellotaIds = mMeshToBellotas[meshId];
    auto pair = bellotaIds.insert(bellotaId);
    debugCheck(pair.second);

    const std::size_t erasedElements = mUnusedMeshes.erase(meshId);
    debugCheck(erasedElements == 1);
    return true;
}

bool MeshUsageMonitor::hasEntry(BellotaId bellotaId, MeshId meshId) const
{
    if (not mMeshToBellotas.contains(meshId))
        return false;

    const std::unordered_set<BellotaId>& bellotaIds = mMeshToBellotas.at(meshId);
    return bellotaIds.contains(bellotaId);
}

bool MeshUsageMonitor::removeEntry(BellotaId bellotaId, MeshId meshId)
{
    if (not hasEntry(bellotaId, meshId))
        return false;

    std::unordered_set<BellotaId>& bellotaIds = mMeshToBellotas[meshId];
    debugCheck(bellotaIds.contains(bellotaId));

    bellotaIds.erase(bellotaId);

    if (bellotaIds.empty())
    {
        const std::size_t elementsErased = mMeshToBellotas.erase(meshId);
        debugCheck(elementsErased == 1);
        auto pair = mUnusedMeshes.insert(meshId);
        debugCheck(pair.second);
    }

    return true;
}

const std::unordered_set<MeshId>& MeshUsageMonitor::getUnusedMeshIds() const
{
    return mUnusedMeshes;
}

void MeshUsageMonitor::clearUnusedMeshIds()
{
    mUnusedMeshes.clear();
}

std::size_t MeshUsageMonitor::loadedMeshes() const
{
    return mUnusedMeshes.size() + mMeshToBellotas.size();
}

}
