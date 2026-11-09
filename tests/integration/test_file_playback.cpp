// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "decode/miniaudio_decoder.hpp"
#include "decode/streaming_source.hpp"
#include "engine/engine.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <vector>

using namespace djw::core;

namespace
{
    const std::filesystem::path audio_dir{DJW_TEST_AUDIO_DIR};
    constexpr std::uint32_t     sample_rate = 48'000;

    std::unique_ptr<decode::MiniaudioDecoder> openDecoder(const std::filesystem::path &file)
    {
        auto decoder = decode::MiniaudioDecoder::open(file, sample_rate);
        REQUIRE_MESSAGE(decoder.has_value(), decoder.error());
        return std::move(*decoder);
    }
} // namespace

TEST_CASE("A deck plays a file: the engine output is the decoded file")
{
    const auto file = audio_dir / "sine_440hz_48k_stereo.wav";

    // Reference: the file decoded directly
    std::vector<float> expected(12'000 * 2);
    REQUIRE(openDecoder(file)->read(expected) == 12'000);

    engine::Engine          engine{{.sample_rate = sample_rate, .max_block_frames = 256}};
    api::CommandQueue      &queue = engine.addCommandQueue();
    decode::StreamingSource source{openDecoder(file), sample_rate};
    const api::DeckId       deck = engine.addDeck(source);
    REQUIRE(queue.tryPush(api::DeckPlay{deck}));

    // A bit longer than the file: after the end the deck plays silence
    std::vector<float> rendered(12'800 * 2);
    for (std::size_t offset = 0; offset < rendered.size(); offset += 256 * 2)
    {
        engine.render(std::span{rendered}.subspan(offset, 256 * 2));
    }

    CHECK(std::vector<float>(rendered.begin(), rendered.begin() + 12'000 * 2) == expected);
    CHECK(std::all_of(rendered.begin() + 12'000 * 2, rendered.end(), [](float value) { return value == 0.0F; }));
    CHECK(source.endOfStream());
    CHECK(engine.deckSnapshot(deck)->read().position_seconds == doctest::Approx(0.25));
}
