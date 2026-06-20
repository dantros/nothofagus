#pragma once

#include "render_snapshot.h"
#include <array>
#include <atomic>
#include <cstdint>

namespace Nothofagus
{

/**
 * @class SnapshotTripleBuffer
 * @brief Lock-free single-producer / single-consumer triple buffer for
 *        `RenderSnapshot` (the sim→render hand-off).
 *
 * The sim thread fills `writeSlot()` then calls `publish()`; the render thread
 * calls `acquire()` then reads `readSlot()`. Neither thread blocks: the producer
 * always owns a private write slot and the consumer always owns a private read
 * slot, while a third "middle" slot is swapped through a single atomic. If the
 * producer outruns the consumer, intermediate snapshots are overwritten —
 * latest-wins / mailbox semantics, which is what a renderer wants (never render a
 * backlog of stale scene states).
 *
 * The three slots keep their `draws`/`rttPasses` allocations across publishes, so
 * steady-state commits do not allocate.
 *
 * Exactly one thread may call `writeSlot()`/`publish()` and exactly one (other)
 * thread may call `acquire()`/`readSlot()`.
 */
class SnapshotTripleBuffer
{
public:
    SnapshotTripleBuffer() = default;

    /// Producer-private slot to fill before publishing.
    RenderSnapshot& writeSlot() { return mBuffers[mWriteIndex]; }

    /// Publish the filled write slot and pick up a fresh private write slot.
    void publish()
    {
        const std::uint32_t published = mWriteIndex | kFreshBit;
        const std::uint32_t previous = mShared.exchange(published, std::memory_order_acq_rel);
        mWriteIndex = previous & kIndexMask;
    }

    /// If a snapshot has been published since the last acquire, swap it into the
    /// read slot and return true. Otherwise leave the read slot unchanged and
    /// return false (the consumer may re-render the previous snapshot).
    bool acquire()
    {
        if ((mShared.load(std::memory_order_acquire) & kFreshBit) == 0)
            return false;
        const std::uint32_t previous = mShared.exchange(mReadIndex, std::memory_order_acq_rel);
        mReadIndex = previous & kIndexMask;
        return true;
    }

    /// Consumer-private slot. Valid after the first `acquire()`; before any
    /// `publish()` it is a default-constructed (empty) snapshot.
    const RenderSnapshot& readSlot() const { return mBuffers[mReadIndex]; }

private:
    static constexpr std::uint32_t kIndexMask = 0x3u;
    static constexpr std::uint32_t kFreshBit  = 0x4u;

    std::array<RenderSnapshot, 3> mBuffers{};
    std::uint32_t mWriteIndex{0};                 ///< producer-owned slot (starts 0)
    std::uint32_t mReadIndex{2};                  ///< consumer-owned slot (starts 2)
    std::atomic<std::uint32_t> mShared{1};        ///< middle slot index (starts 1), + fresh bit when newly published
};

}
