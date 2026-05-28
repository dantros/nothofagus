#pragma once

#include <unordered_map>
#include <unordered_set>
#include <cstddef>
#include "bellota.h"
#include "mesh.h"

namespace Nothofagus
{

template <typename ResourceId>
class ResourceUsageMonitor
{
public:
    ResourceUsageMonitor() = default;

    bool addUnused(ResourceId resourceId);
    bool hasUnused(ResourceId resourceId) const;
    bool has(ResourceId resourceId) const;
    bool removeUnused(ResourceId resourceId);

    bool addEntry(BellotaId bellotaId, ResourceId resourceId);
    bool hasEntry(BellotaId bellotaId, ResourceId resourceId) const;
    bool removeEntry(BellotaId bellotaId, ResourceId resourceId);

    const std::unordered_set<ResourceId>& getUnusedIds() const;
    void clearUnusedIds();
    std::size_t loaded() const;

private:
    std::unordered_map<ResourceId, std::unordered_set<BellotaId>> mResourceToBellotas;
    std::unordered_set<ResourceId> mUnusedResources;
};

extern template class ResourceUsageMonitor<TextureId>;
extern template class ResourceUsageMonitor<MeshId>;

}
