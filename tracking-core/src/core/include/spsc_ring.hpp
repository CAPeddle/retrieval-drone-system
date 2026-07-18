#pragma once

// Plan KTD-2 / U13: the FrameRingBuffer SPSC algorithm generalised for
// trivially-copyable payloads (the TRK-021 snapshot export queue). Same
// freshest-wins semantics as ADR-002's ZMQ_CONFLATE=1: a full ring overwrites
// the OLDEST unread item, so the consumer always works on the freshest window.
// try_push/try_pop never allocate, never block, take no mutex.
//
// Thread contract and formal-race note are inherited verbatim from
// FrameRingBuffer (TRK-005): head_ is producer-owned; tail_ is consumer-owned
// except the producer's full-ring CAS reclaim; a pop whose tail CAS loses the
// race discards its (possibly torn) copy and retries — a torn copy is never
// RETURNED, but ThreadSanitizer will rightly flag the read (recorded
// follow-up: per-slot seqlock). The hammer test in test_spsc_ring.cpp is the
// behavioural guard.

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <vector>

namespace tracking {

template <typename T>
class SpscRing {
    static_assert(std::is_trivially_copyable_v<T>,
                  "SpscRing payloads cross threads by plain copy");

public:
    explicit SpscRing(std::size_t capacity) : slots_(capacity), capacity_(capacity) {}

    SpscRing(const SpscRing&) = delete;
    SpscRing& operator=(const SpscRing&) = delete;

    // Copies `item` into the next slot; never blocks. Returns true when an
    // unread item was overwritten to make room.
    bool try_push(const T& item) {
        const std::uint64_t head = head_.load(std::memory_order_relaxed);
        std::uint64_t tail = tail_.load(std::memory_order_acquire);
        bool dropped = false;
        if (head - tail >= capacity_) {
            if (tail_.compare_exchange_strong(tail, tail + 1,
                                              std::memory_order_acq_rel)) {
                dropped = true;
                drops_.fetch_add(1, std::memory_order_relaxed);
            }
        }
        slots_[head % capacity_] = item;
        head_.store(head + 1, std::memory_order_release);
        return dropped;
    }

    // Copies the oldest item out; std::nullopt when empty. Never blocks.
    std::optional<T> try_pop() {
        while (true) {
            std::uint64_t tail = tail_.load(std::memory_order_relaxed);
            if (tail == head_.load(std::memory_order_acquire)) {
                return std::nullopt;
            }
            T item = slots_[tail % capacity_];
            if (tail_.compare_exchange_strong(tail, tail + 1,
                                              std::memory_order_acq_rel)) {
                return item;
            }
            // Producer reclaimed the slot mid-copy: discard and retry.
        }
    }

    std::size_t capacity() const noexcept { return capacity_; }
    std::uint64_t drops() const noexcept {
        return drops_.load(std::memory_order_relaxed);
    }

private:
    std::vector<T> slots_;
    std::size_t capacity_;
    std::atomic<std::uint64_t> head_{0};
    std::atomic<std::uint64_t> tail_{0};
    std::atomic<std::uint64_t> drops_{0};
};

}  // namespace tracking
