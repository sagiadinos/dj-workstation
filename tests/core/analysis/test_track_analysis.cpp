// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "analysis/track_analysis.hpp"

#include "temp_dir.hpp"
#include "wav_file.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

using namespace djw::core::analysis;

namespace
{
    constexpr std::uint32_t rate         = 48'000;
    constexpr std::size_t   block_frames = 1'024; // the frames of one waveform block

    /// Interleaved stereo, silent for silence_seconds, then a kick (decaying sine) on every beat
    std::vector<float> beatsAfterSilence(double bpm, double silence_seconds, double seconds)
    {
        const auto         frames = static_cast<std::size_t>(seconds * rate);
        std::vector<float> samples(frames * 2, 0.0F);
        const double       beat = 60.0 / bpm * rate;
        for (double start = silence_seconds * rate; start < static_cast<double>(frames); start += beat)
        {
            for (std::size_t i = 0; i < 6'000 && static_cast<std::size_t>(start) + i < frames; ++i)
            {
                const double t = static_cast<double>(i) / rate;
                const auto   value =
                    static_cast<float>(0.8 * std::exp(-t * 40.0) * std::sin(2.0 * std::numbers::pi * 60.0 * t));
                samples[(static_cast<std::size_t>(start) + i) * 2]       = value;
                samples[((static_cast<std::size_t>(start) + i) * 2) + 1] = value;
            }
        }
        return samples;
    }
} // namespace

TEST_CASE("WaveformBuilder: a short track gives one column per block, the loudest sample of the block")
{
    WaveformBuilder    builder;
    std::vector<float> stereo(1'024 * 2 * 3, 0.0F);
    stereo[10]              = 0.5F;  // block 0, left
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

TEST_CASE("WaveformBuilder: the last frame counts, also when it is alone in its block")
{
    WaveformBuilder    builder;
    std::vector<float> stereo((block_frames + 1) * 2, 0.0F);
    stereo.back() = -0.5F; // right channel of the very last frame
    builder.add(stereo);
    CHECK(builder.finish() == std::vector<std::uint8_t>{0, 128});
}

TEST_CASE("WaveformBuilder: frames handed over in pieces count like one piece")
{
    WaveformBuilder    whole;
    WaveformBuilder    pieces;
    std::vector<float> stereo(block_frames * 2 * 3, 0.0F);
    stereo[(block_frames * 2) + 6] = 0.75F;
    whole.add(stereo);
    for (std::size_t at = 0; at < stereo.size(); at += 300)
    {
        pieces.add(std::span{stereo}.subspan(at, std::min<std::size_t>(300, stereo.size() - at)));
    }
    CHECK(pieces.finish() == whole.finish());
}

TEST_CASE("analyzeFile: finds the tempo and where the sound is in a file")
{
    const djw::tests::TempDir folder;
    const auto                file = folder.path() / "beat.wav";
    REQUIRE(djw::tests::writeWav(file, {.sample_rate = rate, .samples = beatsAfterSilence(120.0, 12.0, 36.0)}));

    const std::atomic<bool> stop{false};
    const auto              result = analyzeFile(file, rate, stop);
    REQUIRE(result.has_value());
    REQUIRE(result->bpm.has_value());
    CHECK(*result->bpm == doctest::Approx(120.0).epsilon(0.01));

    // The first third is silent, the rest is not
    REQUIRE(result->waveform.size() == waveform_columns);
    const auto third = result->waveform.size() / 3;
    CHECK(std::ranges::all_of(std::span{result->waveform}.first(third - 2), [](auto value) { return value == 0; }));
    CHECK(*std::ranges::max_element(std::span{result->waveform}.subspan(third + 2)) > 150);
}

TEST_CASE("analyzeFile: reads the waveform of a file, an unknown file is an error")
{
    const std::atomic<bool> stop{false};
    const auto              result =
        analyzeFile(std::filesystem::path{DJW_TEST_AUDIO_DIR} / "sine_440hz_48k_stereo.wav", 48'000, stop);
    REQUIRE(result.has_value());
    CHECK_FALSE(result->waveform.empty());
    CHECK(*std::ranges::max_element(result->waveform) > 0);
    CHECK_FALSE(result->bpm.has_value()); // a sine has no beat

    CHECK_FALSE(analyzeFile("/nonexistent/file.wav", 48'000, stop).has_value());
    const std::atomic<bool> stopped{true};
    CHECK_FALSE(analyzeFile(std::filesystem::path{DJW_TEST_AUDIO_DIR} / "sine_440hz_48k_stereo.wav", 48'000, stopped)
                    .has_value());
}
