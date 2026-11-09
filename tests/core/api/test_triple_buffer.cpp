// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "api/triple_buffer.hpp"

#include <doctest/doctest.h>

#include <atomic>
#include <cstdint>
#include <thread>

using djw::core::api::TripleBuffer;

namespace
{
    // Two fields written together, so a torn read shows up as b != 2 * a
    struct Pair
    {
            std::uint64_t a = 0;
            std::uint64_t b = 0;
    };
} // namespace

TEST_CASE("TripleBuffer: read returns the initial value before anything is written")
{
    TripleBuffer<int> buffer{7};
    CHECK(buffer.read() == 7);
}

TEST_CASE("TripleBuffer: read returns the latest written value")
{
    TripleBuffer<int> buffer{0};
    buffer.write(1);
    CHECK(buffer.read() == 1);
    buffer.write(2);
    buffer.write(3);
    CHECK(buffer.read() == 3);
}

TEST_CASE("TripleBuffer: reading again without a new write keeps the value")
{
    TripleBuffer<int> buffer{0};
    buffer.write(5);
    CHECK(buffer.read() == 5);
    CHECK(buffer.read() == 5);
}

TEST_CASE("TripleBuffer: many writes between reads never return an older value")
{
    TripleBuffer<int> buffer{0};
    for (int i = 1; i <= 100; ++i)
    {
        buffer.write(i);
        buffer.write(i * 1000);
        CHECK(buffer.read() == i * 1000);
    }
}

TEST_CASE("TripleBuffer: concurrent reader sees whole values that never go back in time")
{
    constexpr std::uint64_t count = 200'000;
    TripleBuffer<Pair>      buffer;
    std::atomic<bool>       done{false};

    std::thread writer(
        [&buffer, &done]
        {
            for (std::uint64_t i = 1; i <= count; ++i)
            {
                buffer.write(Pair{i, 2 * i});
            }
            done.store(true, std::memory_order_release);
        });

    bool          consistent = true;
    bool          monotonic  = true;
    std::uint64_t last       = 0;
    while (!done.load(std::memory_order_acquire))
    {
        const Pair value = buffer.read();
        consistent       = consistent && value.b == 2 * value.a;
        monotonic        = monotonic && value.a >= last;
        last             = value.a;
    }
    writer.join();

    CHECK(consistent);
    CHECK(monotonic);
    CHECK(buffer.read().a == count);
}
