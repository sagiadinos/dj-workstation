// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "decode/miniaudio_decoder.hpp"
#include "decode/track_loader.hpp"
#include "engine/engine.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <vector>

using namespace djw::core;

namespace
{
    const std::filesystem::path audio_dir{DJW_TEST_AUDIO_DIR};
    constexpr std::uint32_t     sample_rate = 48'000;
    constexpr std::size_t       file_frames = 12'000;

    std::vector<float> decodeFile(const std::filesystem::path &file)
    {
        auto decoder = decode::MiniaudioDecoder::open(file, sample_rate);
        REQUIRE_MESSAGE(decoder.has_value(), decoder.error());
        std::vector<float> samples(file_frames * 2);
        REQUIRE((*decoder)->read(samples) == file_frames);
        return samples;
    }

    /// Renders frames frames in blocks of 256
    std::vector<float> render(engine::Engine &engine, std::size_t frames)
    {
        std::vector<float> rendered(frames * 2);
        for (std::size_t offset = 0; offset < rendered.size(); offset += 256 * 2)
        {
            engine.render(
                std::span{rendered}.subspan(offset, std::min<std::size_t>(256 * 2, rendered.size() - offset)));
        }
        return rendered;
    }
} // namespace

TEST_CASE("A deck plays a file: the engine output is the decoded file, held in memory or streamed")
{
    const auto file     = audio_dir / "sine_440hz_48k_stereo.wav";
    const auto expected = decodeFile(file);

    // A limit of 0 forces streaming
    for (const std::uint64_t limit : {std::uint64_t{48'000 * 300}, std::uint64_t{0}})
    {
        CAPTURE(limit);
        engine::Engine     engine{{.sample_rate = sample_rate, .max_block_frames = 256}};
        api::CommandQueue &queue = engine.addCommandQueue();
        const api::DeckId  deck  = engine.addDeck();
        auto               track = decode::openTrack(
            file, {.sample_rate = sample_rate, .memory_limit_frames = limit, .stream_buffer_frames = sample_rate});
        REQUIRE(track.has_value());
        REQUIRE(engine.setTrack(deck, std::move(*track)));
        REQUIRE(queue.tryPush(api::DeckPlay{deck}));

        // A bit longer than the file: after the end the deck plays silence and stops
        const auto rendered = render(engine, 12'800);

        CHECK(std::vector<float>(rendered.begin(), rendered.begin() + file_frames * 2) == expected);
        CHECK(
            std::all_of(rendered.begin() + file_frames * 2, rendered.end(), [](float value) { return value == 0.0F; }));
        const api::DeckSnapshot state = engine.deckSnapshot(deck)->read();
        CHECK(state.play_state == api::PlayState::Stopped);
        CHECK(state.position_seconds == doctest::Approx(0.25));
        CHECK(state.duration_seconds == doctest::Approx(0.25));
    }
}

TEST_CASE("Seek in a file held in memory continues exactly at the new position")
{
    const auto file     = audio_dir / "sine_440hz_48k_stereo.flac";
    const auto expected = decodeFile(file);

    engine::Engine     engine{{.sample_rate = sample_rate, .max_block_frames = 256}};
    api::CommandQueue &queue = engine.addCommandQueue();
    const api::DeckId  deck  = engine.addDeck();
    auto               track = decode::openTrack(file, {.sample_rate = sample_rate});
    REQUIRE(track.has_value());
    REQUIRE(engine.setTrack(deck, std::move(*track)));
    REQUIRE(queue.tryPush(api::DeckSeek{deck, 0.125}));
    REQUIRE(queue.tryPush(api::DeckPlay{deck}));

    const auto rendered = render(engine, 1'000);

    CHECK(std::equal(rendered.begin(), rendered.end(), expected.begin() + 6'000 * 2));
}
