#pragma once

// TRK-005: single-producer/single-consumer frame ring buffer between the
// capture thread (producer) and the processing thread (consumer). §7.1: all
// cv::Mat storage is pre-allocated at construction; try_push/try_pop never
// allocate, never block, and take no mutex. Freshest-wins overflow: a full
// buffer drops the OLDEST unread frame (same staleness philosophy as ADR-002's
// ZMQ_CONFLATE=1 — the consumer always works on the freshest window).

#include "frame_metadata.hpp"

#include <opencv2/core.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace tracking {

// Thread contract: exactly one producer thread calls try_push and exactly one
// consumer thread calls try_pop. head_ is producer-owned. tail_ is normally
// consumer-owned, with one documented deviation from a pure Lamport SPSC ring:
// when the buffer is full the producer CAS-advances tail_ to reclaim the
// oldest slot, so tail_ is briefly contended (CAS between the two threads)
// only while the consumer lags. A pop whose tail CAS loses that race discards
// its (possibly torn) copy and retries with the new tail.
class FrameRingBuffer {
public:
    // Pre-allocates `capacity` slots of rows x cols pixels of `type`
    // (cv::Mat type constant, e.g. CV_8UC1). Startup-only; may allocate.
    FrameRingBuffer(std::size_t capacity, int rows, int cols, int type)
        : slots_(capacity), capacity_(capacity) {
        for (Slot& slot : slots_) {
            slot.frame.create(rows, cols, type);
        }
    }

    FrameRingBuffer(const FrameRingBuffer&) = delete;
    FrameRingBuffer& operator=(const FrameRingBuffer&) = delete;

    // Copies frame + metadata into the next slot; never blocks. Returns true
    // when an unread frame was dropped (overwritten) to make room — the
    // producer can aggregate this into its drop statistics. The frame's
    // dimensions and type must match construction (slot copyTo would
    // reallocate otherwise, violating the no-allocation contract).
    bool try_push(const cv::Mat& frame, const FrameMetadata& metadata) {
        const std::uint64_t head = head_.load(std::memory_order_relaxed);
        std::uint64_t tail = tail_.load(std::memory_order_acquire);
        bool dropped = false;
        if (head - tail >= capacity_) {
            // Full: reclaim the oldest slot. A failed CAS means the consumer
            // popped concurrently — space exists either way.
            if (tail_.compare_exchange_strong(tail, tail + 1,
                                              std::memory_order_acq_rel)) {
                dropped = true;
                drops_.fetch_add(1, std::memory_order_relaxed);
            }
        }
        Slot& slot = slots_[head % capacity_];
        frame.copyTo(slot.frame);
        slot.metadata = metadata;
        head_.store(head + 1, std::memory_order_release);
        return dropped;
    }

    // Copies the oldest frame into out_frame (reusing its storage when the
    // caller pre-allocated matching dimensions) and returns its metadata;
    // std::nullopt when empty. Never blocks.
    std::optional<FrameMetadata> try_pop(cv::Mat& out_frame) {
        while (true) {
            std::uint64_t tail = tail_.load(std::memory_order_relaxed);
            if (tail == head_.load(std::memory_order_acquire)) {
                return std::nullopt;
            }
            const Slot& slot = slots_[tail % capacity_];
            slot.frame.copyTo(out_frame);
            const FrameMetadata metadata = slot.metadata;
            // Claim the slot. Losing the CAS means the producer reclaimed it
            // (buffer was full) while we copied — the copy may be torn, so
            // discard it and retry against the advanced tail.
            if (tail_.compare_exchange_strong(tail, tail + 1,
                                              std::memory_order_acq_rel)) {
                return metadata;
            }
        }
    }

    std::size_t capacity() const { return capacity_; }

    // Total frames dropped to overwrite since construction.
    std::uint64_t drops() const { return drops_.load(std::memory_order_relaxed); }

    // Approximate unread count (racy by nature; diagnostics only).
    std::size_t size() const {
        const std::uint64_t head = head_.load(std::memory_order_acquire);
        const std::uint64_t tail = tail_.load(std::memory_order_acquire);
        return head > tail ? static_cast<std::size_t>(head - tail) : 0;
    }

private:
    struct Slot {
        cv::Mat frame;
        FrameMetadata metadata;
    };

    std::vector<Slot> slots_;
    std::size_t capacity_;
    std::atomic<std::uint64_t> head_{0};   // next write index (monotonic)
    std::atomic<std::uint64_t> tail_{0};   // next read index (monotonic)
    std::atomic<std::uint64_t> drops_{0};
};

}  // namespace tracking
