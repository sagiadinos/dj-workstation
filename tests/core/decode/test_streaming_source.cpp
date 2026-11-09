// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "decode/streaming_source.hpp"

#include "allocation_counter.hpp"
#include "fake_decoder.hpp"

#include <doctest/doctest.h>

#include <chrono>
#include <thread>
#include <vector>

using djw::core::decode::StreamingSource;
using djw::tests::AllocationCounter;
using djw::tests::FakeDecoder;

namespace
{
    /// Reads the whole stream in blocks like the audio thread would, waiting while the worker catches up.
    std::vector<float> readAll(StreamingSource &source, std::size_t block_frames)
    {
        std::vector<float> all;
        std::vector<float> block(block_frames * 2);
        const auto         deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
        while (!source.endOfStream() && std::chrono::steady_clock::now() < deadline)
        {
            const std::size_t frames = source.read(block);
            all.insert(all.end(), block.begin(), block.begin() + static_cast<std::ptrdiff_t>(frames * 2));
            if (frames == 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds{1});
            }
        }
        return all;
    }

    bool isRamp(const std::vector<float> &samples)
    {
        for (std::size_t i = 0; i < samples.size(); ++i)
        {
            if (samples[i] != static_cast<float>(i / 2))
            {
                return false;
            }
        }
        return true;
    }
} // namespace

TEST_CASE("StreamingSource: a short stream is buffered completely by the constructor")
{
    StreamingSource source{std::make_unique<FakeDecoder>(100), 1'000};

    std::vector<float> output(300 * 2);
    CHECK(source.read(output) == 100);
    output.resize(200);
    CHECK(isRamp(output));
    CHECK(source.endOfStream());
    CHECK_FALSE(source.failed());
    CHECK(source.read(output) == 0);
}

TEST_CASE("StreamingSource: a stream longer than the buffer arrives complete and in order")
{
    constexpr std::size_t total_frames = 200'000;
    StreamingSource       source{std::make_unique<FakeDecoder>(total_frames), 4'096};

    const auto samples = readAll(source, 256);

    CHECK(samples.size() == total_frames * 2);
    CHECK(isRamp(samples));
    CHECK(source.endOfStream());
}

TEST_CASE("StreamingSource: a decoder error ends the stream after the frames decoded so far")
{
    StreamingSource source{std::make_unique<FakeDecoder>(50'000, 10'000), 2'048};

    const auto samples = readAll(source, 256);

    CHECK(samples.size() == 10'000 * 2);
    CHECK(isRamp(samples));
    CHECK(source.endOfStream());
    CHECK(source.failed());
}

TEST_CASE("StreamingSource: read does not allocate or free memory")
{
    StreamingSource    source{std::make_unique<FakeDecoder>(10'000), 20'000};
    std::vector<float> output(256 * 2);

    const AllocationCounter counter;
    for (int i = 0; i < 20; ++i)
    {
        source.read(output);
    }
    CHECK(counter.allocations() == 0);
    CHECK(counter.deallocations() == 0);
}

TEST_CASE("StreamingSource: can be destroyed while the worker is still decoding")
{
    const auto start = std::chrono::steady_clock::now();
    {
        StreamingSource    source{std::make_unique<FakeDecoder>(10'000'000), 4'096};
        std::vector<float> output(256 * 2);
        source.read(output);
    }
    CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds{2});
}
