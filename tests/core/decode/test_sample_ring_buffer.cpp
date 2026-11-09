// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "decode/sample_ring_buffer.hpp"

#include <doctest/doctest.h>

#include <thread>
#include <vector>

using djw::core::decode::SampleRingBuffer;

TEST_CASE("SampleRingBuffer: capacity is rounded up to a power of two")
{
    CHECK(SampleRingBuffer{5}.capacity() == 8);
    CHECK(SampleRingBuffer{8}.capacity() == 8);
    CHECK(SampleRingBuffer{0}.capacity() == 2);
}

TEST_CASE("SampleRingBuffer: write copies only what fits, read only what is there")
{
    SampleRingBuffer         buffer{4};
    const std::vector<float> input{1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F};

    CHECK(buffer.write(input) == 4);
    CHECK(buffer.writeAvailable() == 0);
    CHECK(buffer.readAvailable() == 4);

    std::vector<float> output(6, 0.0F);
    CHECK(buffer.read(output) == 4);
    CHECK(output == std::vector<float>{1.0F, 2.0F, 3.0F, 4.0F, 0.0F, 0.0F});
    CHECK(buffer.read(output) == 0);
}

TEST_CASE("SampleRingBuffer: data stays in order across the wrap-around")
{
    SampleRingBuffer   buffer{8};
    std::vector<float> output(3);
    float              next_write = 0.0F;
    float              next_read  = 0.0F;
    for (int round = 0; round < 50; ++round)
    {
        const std::vector<float> input{next_write, next_write + 1.0F, next_write + 2.0F};
        REQUIRE(buffer.write(input) == 3);
        next_write += 3.0F;

        REQUIRE(buffer.read(output) == 3);
        for (const float value : output)
        {
            REQUIRE(value == next_read);
            next_read += 1.0F;
        }
    }
}

TEST_CASE("SampleRingBuffer: a writer and a reader thread transfer everything in order")
{
    constexpr std::size_t total = 500'000;
    SampleRingBuffer      buffer{1'024};

    std::thread writer(
        [&buffer]
        {
            std::vector<float> block(100);
            std::size_t        next = 0;
            while (next < total)
            {
                const std::size_t count = std::min(block.size(), total - next);
                for (std::size_t i = 0; i < count; ++i)
                {
                    block[i] = static_cast<float>((next + i) % 1'000'000);
                }
                std::size_t written = 0;
                while (written < count)
                {
                    written += buffer.write(std::span{block}.subspan(written, count - written));
                    std::this_thread::yield();
                }
                next += count;
            }
        });

    std::vector<float> block(77);
    std::size_t        received = 0;
    bool               in_order = true;
    while (received < total)
    {
        const std::size_t count = buffer.read(block);
        for (std::size_t i = 0; i < count; ++i)
        {
            in_order = in_order && block[i] == static_cast<float>((received + i) % 1'000'000);
        }
        received += count;
        if (count == 0)
        {
            std::this_thread::yield();
        }
    }
    writer.join();

    CHECK(in_order);
    CHECK(buffer.readAvailable() == 0);
}
