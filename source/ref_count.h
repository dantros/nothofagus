#pragma once

#include <cstddef>
#include <unordered_map>

namespace Nothofagus
{

/**
 * @brief Minimal reference counter keyed by an engine id (e.g. TextureId, MeshId).
 *
 * Sibling to ResourceUsageMonitor: where that tracks *which* referrers use a
 * resource (a referrer set, surfaced as an "unused" set for a deferred GC sweep),
 * this is the simpler primitive — an integer count per id, with hooks fired on the
 * 0 -> 1 (first retain) and 1 -> 0 (last release) transitions. It lets a caller pin
 * an underlying resource exactly once while several local entries share it, and
 * unpin it exactly once when the last entry goes away.
 *
 * @tparam Id An id type exposing a `std::size_t id` member.
 */
template <typename Id>
class RefCount
{
public:
    /// Increment the count for `id`; invoke `onFirst(id)` on the 0 -> 1 transition.
    template <typename OnFirst>
    void retain(Id id, OnFirst&& onFirst)
    {
        if (mCounts[id.id]++ == 0)
            onFirst(id);
    }

    /// Decrement the count for `id`; invoke `onLast(id)` on the 1 -> 0 transition
    /// (and forget the id). No-op if `id` is not currently counted.
    template <typename OnLast>
    void release(Id id, OnLast&& onLast)
    {
        auto it = mCounts.find(id.id);
        if (it == mCounts.end() || --it->second != 0)
            return;
        mCounts.erase(it);
        onLast(id);
    }

    /// Drop all counts without firing hooks (teardown).
    void clear() { mCounts.clear(); }

private:
    std::unordered_map<std::size_t, int> mCounts;
};

}
