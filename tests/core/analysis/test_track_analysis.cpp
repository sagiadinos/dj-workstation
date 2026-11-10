// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "analysis/track_analysis.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <vector>

using namespace djw::core::analysis;

TEST_CASE("WaveformBuilder: a short track gives one column per block, the loudest sample of the block")
{
    WaveformBuilder    builder;
    std::vector<float> stereo(1'024 * 2 * 3, 0.0F);
    stereo[10]             = 0.5F;  // block 0, left
    stereo[(1'024 * 2) + 3] = -1.0F; // block 1, right
    builder.add(stereo);
    CHECK(builder.finish() == std::vector<std::uint8_t>{128, 255, 0});
}

TEST_CASE("WaveformBuilder: a long track is folded into waveform_columns columns by the loudest block")
{
    WaveformBuilder    builder;
    const std::size_t  blocks = waveform_columns * 4;
    std::vector<float> stereo(1'024 * 2 * blocks, 0.0F);
    stereo[1'024 * 2 * 7] = 0.25F; // block 7 lies in column 1
    builder.add(stereo);
    const auto waveform = builder.finish();
    REQUIRE(waveform.size() == waveform_columns);
    CHECK(waveform[1] == 64);
    CHECK(waveform[0] == 0);
    CHECK(waveform[2] == 0);
}

TEST_CASE("WaveformBuilder: an incomplete last block counts, nothing gives nothing")
{
    CHECK(WaveformBuilder{}.finish().empty());
    WaveformBuilder    builder;
    std::vector<float> stereo(100 * 2, 0.0F);
    stereo[0] = 1.0F;
    builder.add(stereo);
    CHECK(builder.finish() == std::vector<std::uint8_t>{255});
}

TEST_CASE("analyzeFile: reads the waveform of a file, an unknown file is an error")
{
    const std::atomic<bool> stop{false};
    const auto result = analyzeFile(std::filesystem::path{DJW_TEST_AUDIO_DIR} / "sine_440hz_48k_stereo.wav", 48'000, stop);
    REQUIRE(result.has_value());
    CHECK_FALSE(result->waveform.empty());
    CHECK(*std::ranges::max_element(result->waveform) > 0);
    CHECK_FALSE(result->bpm.has_value()); // a sine has no beat

    CHECK_FALSE(analyzeFile("/nonexistent/file.wav", 48'000, stop).has_value());
    const std::atomic<bool> stopped{true};
    CHECK_FALSE(analyzeFile(std::filesystem::path{DJW_TEST_AUDIO_DIR} / "sine_440hz_48k_stereo.wav", 48'000, stopped)
                    .has_value());
}
