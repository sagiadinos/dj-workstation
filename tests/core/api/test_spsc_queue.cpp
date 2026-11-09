// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "api/spsc_queue.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <thread>

using djw::core::api::SpscQueue;

TEST_CASE("SpscQueue: pop on an empty queue returns nothing")
{
    SpscQueue<int, 4> queue;
    CHECK_FALSE(queue.tryPop().has_value());
}

TEST_CASE("SpscQueue: values come out in the order they went in")
{
    SpscQueue<int, 4> queue;
    REQUIRE(queue.tryPush(1));
    REQUIRE(queue.tryPush(2));
    REQUIRE(queue.tryPush(3));

    CHECK(queue.tryPop() == 1);
    CHECK(queue.tryPop() == 2);
    CHECK(queue.tryPop() == 3);
    CHECK_FALSE(queue.tryPop().has_value());
}

TEST_CASE("SpscQueue: all slots are usable and a full queue rejects the push")
{
    SpscQueue<int, 4> queue;
    for (int i = 0; i < 4; ++i)
    {
        REQUIRE(queue.tryPush(i));
    }
    CHECK_FALSE(queue.tryPush(99));

    CHECK(queue.tryPop() == 0);
    CHECK(queue.tryPush(4));
    CHECK(queue.tryPop() == 1);
}

TEST_CASE("SpscQueue: keeps the order across many wrap-arounds")
{
    SpscQueue<int, 4> queue;
    int               next_expected = 0;
    for (int i = 0; i < 1000; ++i)
    {
        REQUIRE(queue.tryPush(i));
        if (i % 3 == 2)
        {
            while (auto value = queue.tryPop())
            {
                REQUIRE(*value == next_expected);
                ++next_expected;
            }
        }
    }
    while (auto value = queue.tryPop())
    {
        REQUIRE(*value == next_expected);
        ++next_expected;
    }
    CHECK(next_expected == 1000);
}

TEST_CASE("SpscQueue: one producer and one consumer thread lose and reorder nothing")
{
    constexpr std::uint32_t      count = 200'000;
    SpscQueue<std::uint32_t, 64> queue;

    std::thread producer(
        [&queue]
        {
            for (std::uint32_t i = 0; i < count; ++i)
            {
                while (!queue.tryPush(i))
                {
                    std::this_thread::yield();
                }
            }
        });

    std::uint32_t next_expected = 0;
    bool          in_order      = true;
    while (next_expected < count)
    {
        if (auto value = queue.tryPop())
        {
            in_order = in_order && *value == next_expected;
            ++next_expected;
        }
        else
        {
            std::this_thread::yield();
        }
    }
    producer.join();

    CHECK(in_order);
    CHECK_FALSE(queue.tryPop().has_value());
}
