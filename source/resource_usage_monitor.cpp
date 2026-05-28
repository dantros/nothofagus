
#include "resource_usage_monitor.h"
#include "check.h"

namespace Nothofagus
{

template <typename ResourceId>
bool ResourceUsageMonitor<ResourceId>::addUnused(ResourceId resourceId)
{
    auto pair = mUnusedResources.insert(resourceId);
    return pair.second;
}

template <typename ResourceId>
bool ResourceUsageMonitor<ResourceId>::hasUnused(ResourceId resourceId) const
{
    return mUnusedResources.contains(resourceId);
}

template <typename ResourceId>
bool ResourceUsageMonitor<ResourceId>::has(ResourceId resourceId) const
{
    return mUnusedResources.contains(resourceId) or mResourceToBellotas.contains(resourceId);
}

template <typename ResourceId>
bool ResourceUsageMonitor<ResourceId>::removeUnused(ResourceId resourceId)
{
    if (not mUnusedResources.contains(resourceId))
        return false;

    const std::size_t elementsErased = mUnusedResources.erase(resourceId);
    return elementsErased == 1;
}

template <typename ResourceId>
bool ResourceUsageMonitor<ResourceId>::addEntry(BellotaId bellotaId, ResourceId resourceId)
{
    if (mResourceToBellotas.contains(resourceId))
    {
        debugCheck(not mUnusedResources.contains(resourceId));
        std::unordered_set<BellotaId>& bellotaIds = mResourceToBellotas[resourceId];
        auto pair = bellotaIds.insert(bellotaId);
        return pair.second;
    }

    debugCheck(mUnusedResources.contains(resourceId));

    mResourceToBellotas.emplace(resourceId, std::unordered_set<BellotaId>{});
    std::unordered_set<BellotaId>& bellotaIds = mResourceToBellotas[resourceId];
    auto pair = bellotaIds.insert(bellotaId);
    debugCheck(pair.second);

    const std::size_t erasedElements = mUnusedResources.erase(resourceId);
    debugCheck(erasedElements == 1);
    return true;
}

template <typename ResourceId>
bool ResourceUsageMonitor<ResourceId>::hasEntry(BellotaId bellotaId, ResourceId resourceId) const
{
    if (not mResourceToBellotas.contains(resourceId))
        return false;

    const std::unordered_set<BellotaId>& bellotaIds = mResourceToBellotas.at(resourceId);
    return bellotaIds.contains(bellotaId);
}

template <typename ResourceId>
bool ResourceUsageMonitor<ResourceId>::removeEntry(BellotaId bellotaId, ResourceId resourceId)
{
    if (not hasEntry(bellotaId, resourceId))
        return false;

    std::unordered_set<BellotaId>& bellotaIds = mResourceToBellotas[resourceId];
    debugCheck(bellotaIds.contains(bellotaId));

    bellotaIds.erase(bellotaId);

    if (bellotaIds.empty())
    {
        const std::size_t elementsErased = mResourceToBellotas.erase(resourceId);
        debugCheck(elementsErased == 1);
        auto pair = mUnusedResources.insert(resourceId);
        debugCheck(pair.second);
    }

    return true;
}

template <typename ResourceId>
const std::unordered_set<ResourceId>& ResourceUsageMonitor<ResourceId>::getUnusedIds() const
{
    return mUnusedResources;
}

template <typename ResourceId>
void ResourceUsageMonitor<ResourceId>::clearUnusedIds()
{
    mUnusedResources.clear();
}

template <typename ResourceId>
std::size_t ResourceUsageMonitor<ResourceId>::loaded() const
{
    return mUnusedResources.size() + mResourceToBellotas.size();
}

template class ResourceUsageMonitor<TextureId>;
template class ResourceUsageMonitor<MeshId>;

}
