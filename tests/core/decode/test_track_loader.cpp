// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "decode/track_loader.hpp"

#include "decode/memory_track.hpp"
#include "decode/streaming_source.hpp"
#include "fake_decoder.hpp"

#include <doctest/doctest.h>

#include <filesystem>

using namespace djw::core::decode;
using djw::tests::FakeDecoder;
using djw::tests::FakeDecoderOptions;

namespace
{
    const TrackLoadConfig config{.sample_rate = 48'000, .memory_limit_frames = 10'000, .stream_buffer_frames = 4'096};
} // namespace

TEST_CASE("makeTrack: a track within the limit is held in memory")
{
    const auto track = makeTrack(std::make_unique<FakeDecoder>(10'000), config);
    CHECK(dynamic_cast<MemoryTrack *>(track.get()) != nullptr);
}

TEST_CASE("makeTrack: a track above the limit is streamed")
{
    const auto track = makeTrack(std::make_unique<FakeDecoder>(10'001), config);
    CHECK(dynamic_cast<StreamingSource *>(track.get()) != nullptr);
}

TEST_CASE("makeTrack: a track of unknown length is streamed")
{
    auto       decoder = std::make_unique<FakeDecoder>(FakeDecoderOptions{.total_frames = 0});
    const auto track   = makeTrack(std::move(decoder), config);
    CHECK(dynamic_cast<StreamingSource *>(track.get()) != nullptr);
}

TEST_CASE("openTrack: a file that cannot be decoded is an error naming the file")
{
    const auto track = openTrack(std::filesystem::path{DJW_TEST_AUDIO_DIR} / "missing.flac", config);
    REQUIRE_FALSE(track.has_value());
    CHECK(track.error().find("missing.flac") != std::string::npos);
}
