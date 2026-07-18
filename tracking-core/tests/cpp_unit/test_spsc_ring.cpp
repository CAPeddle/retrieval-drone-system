// Plan U13: SpscRing tests — ordering, freshest-wins overwrite, and the
// producer/consumer hammer test guarding the CAS-reclaim algorithm (no torn
// or duplicated item may ever be RETURNED).

#include "spsc_ring.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <thread>

namespace {

struct Payload {
    std::uint64_t value = 0;
    std::uint64_t check = 0;  // must always equal value * kMagic
};
constexpr std::uint64_t kMagic = 0x9E3779B97F4A7C15ULL;

Payload make_payload(std::uint64_t value) { return {value, value * kMagic}; }

TEST(SpscRingTest, PopsInPushOrder) {
    tracking::SpscRing<Payload> ring(4);
    for (std::uint64_t i = 0; i < 3; ++i) {
        EXPECT_FALSE(ring.try_push(make_payload(i)));
    }
    for (std::uint64_t i = 0; i < 3; ++i) {
        const auto item = ring.try_pop();
        ASSERT_TRUE(item.has_value());
        EXPECT_EQ(item->value, i);
    }
    EXPECT_FALSE(ring.try_pop().has_value());
}

TEST(SpscRingTest, OverwriteWhenFullKeepsFreshest) {
    tracking::SpscRing<Payload> ring(2);
    ring.try_push(make_payload(1));
    ring.try_push(make_payload(2));
    EXPECT_TRUE(ring.try_push(make_payload(3)));  // overwrites 1
    EXPECT_EQ(ring.drops(), 1U);

    const auto first = ring.try_pop();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->value, 2U);  // oldest unread after the reclaim
    const auto second = ring.try_pop();
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second->value, 3U);
}

TEST(SpscRingTest, HammerNeverReturnsTornOrStaleItems) {
    // One producer pushing flat out into a tiny ring, one consumer draining:
    // every popped item must be internally consistent (check == value*magic)
    // and values must be strictly increasing (a subsequence, never a repeat).
    tracking::SpscRing<Payload> ring(3);
    constexpr std::uint64_t kCount = 1'000'000;

    std::thread producer([&ring] {
        for (std::uint64_t i = 1; i <= kCount; ++i) {
            ring.try_push(make_payload(i));
        }
    });

    std::uint64_t last = 0;
    std::uint64_t popped = 0;
    bool torn = false;
    bool out_of_order = false;
    std::thread consumer([&] {
        while (last < kCount) {
            const auto item = ring.try_pop();
            if (!item.has_value()) {
                std::this_thread::yield();
                continue;
            }
            ++popped;
            if (item->check != item->value * kMagic) {
                torn = true;
                break;
            }
            if (item->value <= last) {
                out_of_order = true;
                break;
            }
            last = item->value;
        }
    });

    producer.join();
    consumer.join();
    EXPECT_FALSE(torn) << "torn payload returned";
    EXPECT_FALSE(out_of_order) << "duplicate or reordered payload returned";
    EXPECT_EQ(last, kCount);  // the final item always survives (freshest-wins)
    EXPECT_GT(popped, 0U);
}

}  // namespace
