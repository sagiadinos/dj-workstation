// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "decode/streaming_source.hpp"

#include "allocation_counter.hpp"
#include "fake_decoder.hpp"
#include "read_helpers.hpp"

#include <doctest/doctest.h>

#include <chrono>
#include <vector>

using djw::core::decode::StreamingSource;
using djw::tests::AllocationCounter;
using djw::tests::FakeDecoder;
using djw::tests::FakeDecoderOptions;
using djw::tests::isRampFrom;
using djw::tests::readAll;
using djw::tests::readFrames;

TEST_CASE("StreamingSource: a short stream is buffered completely by the constructor")
{
    StreamingSource source{std::make_unique<FakeDecoder>(100), 1'000};

    std::vector<float> output(300 * 2);
    CHECK(source.read(output) == 100);
    output.resize(200);
    CHECK(isRampFrom(output, 0));
    CHECK(source.atEnd());
    CHECK_FALSE(source.failed());
    CHECK(source.lengthFrames() == 100);
}

TEST_CASE("StreamingSource: a stream longer than the buffer arrives complete and in order")
{
    constexpr std::size_t total_frames = 200'000;
    StreamingSource       source{std::make_unique<FakeDecoder>(total_frames), 4'096};

    const auto samples = readAll(source);

    CHECK(samples.size() == total_frames * 2);
    CHECK(isRampFrom(samples, 0));
    CHECK(source.atEnd());
}

TEST_CASE("StreamingSource: a decoder error ends the stream after the frames decoded so far")
{
    StreamingSource source{
        std::make_unique<FakeDecoder>(FakeDecoderOptions{.total_frames = 50'000, .fail_at_frame = 10'000}), 2'048};

    const auto samples = readAll(source);

    CHECK(samples.size() == 10'000 * 2);
    CHECK(isRampFrom(samples, 0));
    CHECK(source.atEnd());
    CHECK(source.failed());
}

TEST_CASE("StreamingSource: after a seek only audio from the new position comes")
{
    StreamingSource source{std::make_unique<FakeDecoder>(200'000), 4'096};
    static_cast<void>(readFrames(source, 1'000));

    SUBCASE("forward")
    {
        source.seek(150'000);
        CHECK(isRampFrom(readFrames(source, 10'000), 150'000));
    }
    SUBCASE("backward")
    {
        source.seek(10);
        CHECK(isRampFrom(readFrames(source, 10'000), 10));
    }
    SUBCASE("several seeks in a row, the last one counts")
    {
        source.seek(50'000);
        source.seek(70'000);
        source.seek(30'000);
        CHECK(isRampFrom(readFrames(source, 10'000), 30'000));
    }
}

TEST_CASE("StreamingSource: audio decoded for the old position never leaks after a seek")
{
    // A slow decoder and a reader that keeps the ring nearly empty: the worker is usually in the middle of decoding
    // a chunk for the old position when the seek comes, and writes it afterwards.
    StreamingSource source{std::make_unique<FakeDecoder>(FakeDecoderOptions{
                               .total_frames = 10'000'000, .read_delay = std::chrono::microseconds{300}}),
                           2'048};

    for (std::uint64_t round = 0; round < 60; ++round)
    {
        static_cast<void>(readFrames(source, 700));
        const std::uint64_t target = (round * 7'919 % 97) * 100'000;
        source.seek(target);
        const auto samples = readFrames(source, 1'500);
        REQUIRE(samples.size() == 1'500 * 2);
        REQUIRE(isRampFrom(samples, target));
    }
}

TEST_CASE("StreamingSource: a seek after the end plays again from there")
{
    StreamingSource source{std::make_unique<FakeDecoder>(5'000), 8'192};
    static_cast<void>(readAll(source));
    REQUIRE(source.atEnd());

    source.seek(1'000);

    CHECK_FALSE(source.atEnd());
    const auto samples = readAll(source);
    CHECK(samples.size() == 4'000 * 2);
    CHECK(isRampFrom(samples, 1'000));
}

TEST_CASE("StreamingSource: seek is clamped to the length")
{
    StreamingSource source{std::make_unique<FakeDecoder>(5'000), 8'192};

    source.seek(99'000);

    CHECK(readAll(source).empty());
    CHECK(source.atEnd());
}

TEST_CASE("StreamingSource: read and seek do not allocate or free memory")
{
    StreamingSource    source{std::make_unique<FakeDecoder>(100'000), 20'000};
    std::vector<float> output(256 * 2);

    const AllocationCounter counter;
    for (int i = 0; i < 20; ++i)
    {
        static_cast<void>(source.read(output));
        source.seek(static_cast<std::uint64_t>(i) * 1'000);
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
        static_cast<void>(source.read(output));
    }
    CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds{2});
}
