#pragma once

#include <unordered_map>
#include <unordered_set>
#include <cstddef>
#include "bellota.h"
#include "mesh.h"
#include "check.h"

namespace Nothofagus
{

template <typename ResourceId>
class UsageMonitor
{
public:
    UsageMonitor() = default;

    bool addUnused(ResourceId resourceId)
    {
        auto pair = mUnusedResources.insert(resourceId);
        return pair.second;
    }

    bool hasUnused(ResourceId resourceId) const
    {
        return mUnusedResources.contains(resourceId);
    }

    bool has(ResourceId resourceId) const
    {
        return mUnusedResources.contains(resourceId) or mResourceToBellotas.contains(resourceId);
    }

    bool removeUnused(ResourceId resourceId)
    {
        if (not mUnusedResources.contains(resourceId))
            return false;

        const std::size_t elementsErased = mUnusedResources.erase(resourceId);
        return elementsErased == 1;
    }

    bool addEntry(BellotaId bellotaId, ResourceId resourceId)
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

    bool hasEntry(BellotaId bellotaId, ResourceId resourceId) const
    {
        if (not mResourceToBellotas.contains(resourceId))
            return false;

        const std::unordered_set<BellotaId>& bellotaIds = mResourceToBellotas.at(resourceId);
        return bellotaIds.contains(bellotaId);
    }

    bool removeEntry(BellotaId bellotaId, ResourceId resourceId)
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

    const std::unordered_set<ResourceId>& getUnusedIds() const
    {
        return mUnusedResources;
    }

    void clearUnusedIds()
    {
        mUnusedResources.clear();
    }

    std::size_t loaded() const
    {
        return mUnusedResources.size() + mResourceToBellotas.size();
    }

private:
    std::unordered_map<ResourceId, std::unordered_set<BellotaId>> mResourceToBellotas;
    std::unordered_set<ResourceId> mUnusedResources;
};

using TextureUsageMonitor = UsageMonitor<TextureId>;
using MeshUsageMonitor    = UsageMonitor<MeshId>;

}
