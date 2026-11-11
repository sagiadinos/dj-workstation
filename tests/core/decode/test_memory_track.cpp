// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "decode/memory_track.hpp"

#include "allocation_counter.hpp"
#include "fake_decoder.hpp"
#include "read_helpers.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

using djw::core::decode::MemoryTrack;
using djw::tests::AllocationCounter;
using djw::tests::FakeDecoder;
using djw::tests::FakeDecoderOptions;
using djw::tests::isRampFrom;
using djw::tests::readAll;
using djw::tests::readFrames;

namespace
{
    std::unique_ptr<MemoryTrack> makeTrack(const FakeDecoderOptions &options, std::size_t chunk_frames,
                                           std::shared_ptr<std::atomic<int>> *seeks = nullptr)
    {
        auto decoder = std::make_unique<FakeDecoder>(options);
        if (seeks != nullptr)
        {
            *seeks = decoder->seekCounter();
        }
        auto track = MemoryTrack::create(
            std::move(decoder), options.announced_frames != 0 ? options.announced_frames : options.total_frames,
            chunk_frames);
        REQUIRE(track.has_value());
        return std::move(*track);
    }

    bool waitFor(const MemoryTrack &track, std::uint64_t frame)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
        while (!track.isDecoded(frame) && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
        return track.isDecoded(frame);
    }

    bool waitForAll(const MemoryTrack &track)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
        while (!track.fullyDecoded() && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
        return track.fullyDecoded();
    }
} // namespace

TEST_CASE("MemoryTrack: the first chunk is ready when create returns")
{
    auto track = makeTrack({.total_frames = 10'000, .read_delay = std::chrono::milliseconds{5}}, 1'000);

    CHECK(track->isDecoded(0));
    CHECK(track->isDecoded(999));
    std::vector<float> output(1'000 * 2);
    CHECK(track->read(output) == 1'000);
    CHECK(isRampFrom(output, 0));
}

TEST_CASE("MemoryTrack: the whole track is decoded in the background and reads back exactly")
{
    auto track = makeTrack({.total_frames = 100'000}, 4'096);

    REQUIRE(waitForAll(*track));
    const auto samples = readAll(*track);

    CHECK(samples.size() == 100'000 * 2);
    CHECK(isRampFrom(samples, 0));
    CHECK(track->atEnd());
    CHECK_FALSE(track->failed());
}

TEST_CASE("MemoryTrack: a decoder that gives a few frames per read still fills every chunk exactly")
{
    auto track = makeTrack({.total_frames = 10'000, .max_frames_per_read = 300}, 1'000);
    REQUIRE(waitForAll(*track));
    const auto samples = readAll(*track);
    CHECK(samples.size() == 10'000 * 2);
    CHECK(isRampFrom(samples, 0));
    CHECK_FALSE(track->failed());
}

TEST_CASE("MemoryTrack: decoding in order goes on where the decoder is, without seeking")
{
    std::shared_ptr<std::atomic<int>> seeks;
    auto                              track = makeTrack({.total_frames = 50'000}, 1'000, &seeks);
    REQUIRE(waitForAll(*track));
    CHECK(seeks->load() <= 1); // at most the one to the start
}

TEST_CASE("MemoryTrack: decoded parts can be jumped to in any order without delay")
{
    auto track = makeTrack({.total_frames = 50'000}, 4'096);
    REQUIRE(waitForAll(*track));
    std::vector<float> output(100 * 2);

    for (const std::uint64_t position : {40'000U, 10U, 49'950U, 4'095U, 0U})
    {
        track->seek(position);
        REQUIRE(track->read(output) == 100 - (position == 49'950U ? 50 : 0));
        CHECK(output[0] == static_cast<float>(position));
    }
}

TEST_CASE("MemoryTrack: a seek into a part not decoded yet is decoded first")
{
    // 100 chunks at 2 ms each take 200 ms in order; the last chunk must be there long before
    auto track = makeTrack({.total_frames = 100'000, .read_delay = std::chrono::milliseconds{2}}, 1'000);

    track->seek(95'000);
    std::vector<float> output(10 * 2);
    CHECK(track->read(output) == 0); // not decoded yet: nothing, not the wrong audio

    REQUIRE(waitFor(*track, 95'000));
    CHECK_FALSE(track->fullyDecoded());
    CHECK(track->read(output) == 10);
    CHECK(isRampFrom(output, 95'000));

    // Then the rest, the parts before the seek included
    REQUIRE(waitForAll(*track));
    track->seek(0);
    CHECK(isRampFrom(readAll(*track), 0));
}

TEST_CASE("MemoryTrack: a stream shorter than announced ends in silence")
{
    auto track = makeTrack({.total_frames = 3'000, .announced_frames = 5'000}, 1'000);
    REQUIRE(waitForAll(*track));

    const auto samples = readAll(*track);

    REQUIRE(samples.size() == 5'000 * 2);
    CHECK(isRampFrom(std::vector<float>(samples.begin(), samples.begin() + 3'000 * 2), 0));
    CHECK(std::all_of(samples.begin() + 3'000 * 2, samples.end(), [](float value) { return value == 0.0F; }));
}

TEST_CASE("MemoryTrack: a decoder error leaves silence and is reported")
{
    auto track = makeTrack({.total_frames = 5'000, .fail_at_frame = 2'500}, 1'000);
    REQUIRE(waitForAll(*track));

    const auto samples = readAll(*track);

    CHECK(track->failed());
    CHECK(samples.size() == 5'000 * 2);
    CHECK(isRampFrom(std::vector<float>(samples.begin(), samples.begin() + 2'500 * 2), 0));
}

TEST_CASE("MemoryTrack: seek is clamped to the length")
{
    auto track = makeTrack({.total_frames = 5'000}, 1'000);

    track->seek(9'999);

    CHECK(track->atEnd());
}

TEST_CASE("MemoryTrack: read and seek do not allocate or free memory")
{
    auto track = makeTrack({.total_frames = 50'000}, 4'096);
    REQUIRE(waitForAll(*track));
    std::vector<float> output(256 * 2);

    const AllocationCounter counter;
    for (int i = 0; i < 20; ++i)
    {
        static_cast<void>(track->read(output));
        track->seek(static_cast<std::uint64_t>(i) * 2'000);
    }
    CHECK(counter.allocations() == 0);
    CHECK(counter.deallocations() == 0);
}

TEST_CASE("MemoryTrack: can be destroyed while the worker is still decoding")
{
    const auto start = std::chrono::steady_clock::now();
    {
        auto track = makeTrack({.total_frames = 1'000'000, .read_delay = std::chrono::milliseconds{1}}, 1'000);
    }
    CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds{2});
}

TEST_CASE("MemoryTrack: the decoded fraction grows from the first chunk to the whole track")
{
    // Slow decoding, so the fraction can be seen on its way
    auto        track = makeTrack({.total_frames = 10'000, .read_delay = std::chrono::milliseconds{5}}, 1'000);
    const float first = track->decodedFraction();
    CHECK(first > 0.0F);
    CHECK(first < 1.0F);

    REQUIRE(waitForAll(*track));
    CHECK(track->decodedFraction() == doctest::Approx(1.0F));
}
