// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "api/track_peaks.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <numbers>
#include <vector>

using djw::core::api::TrackPeaks;
using Bands = TrackPeaks::Bands;

namespace
{
    constexpr std::size_t   per_peak = TrackPeaks::frames_per_peak;
    constexpr std::uint32_t rate     = 48'000;

    /// frames of a sine at frequency (Hz) and level, interleaved stereo, both channels alike
    std::vector<float> sine(double frequency, std::size_t frames, float level = 0.8F, std::size_t start = 0)
    {
        std::vector<float> samples(frames * 2);
        for (std::size_t frame = 0; frame < frames; ++frame)
        {
            const auto value = static_cast<float>(
                level * std::sin(2.0 * std::numbers::pi * frequency * static_cast<double>(start + frame) / rate));
            samples[frame * 2]       = value;
            samples[(frame * 2) + 1] = value;
        }
        return samples;
    }

    /// The bands of a second of a sine, read from a value in its second half (the filters have settled)
    Bands bandsOf(double frequency)
    {
        TrackPeaks peaks{rate, rate};
        peaks.add(0, sine(frequency, rate));
        return peaks.at(150);
    }
} // namespace

TEST_CASE("TrackPeaks: one value for every frames_per_peak frames, the last one for what is left")
{
    CHECK(TrackPeaks{0}.size() == 0);
    CHECK(TrackPeaks{1}.size() == 1);
    CHECK(TrackPeaks{per_peak}.size() == 1);
    CHECK(TrackPeaks{per_peak + 1}.size() == 2);
    CHECK(TrackPeaks{10 * per_peak}.size() == 10);
}

TEST_CASE("TrackPeaks: nothing added reads 0, outside the track too")
{
    const TrackPeaks peaks{4 * per_peak};
    for (std::size_t index = 0; index < 4; ++index)
    {
        CHECK(peaks.at(index) == Bands{});
    }
    CHECK(peaks.at(4) == Bands{});
    CHECK(peaks.at(1'000'000) == Bands{});
}

TEST_CASE("TrackPeaks: stored bands only rise, each on its own; outside the track nothing is stored")
{
    TrackPeaks peaks{2 * per_peak};
    peaks.store(0, {.low = 10, .mid = 200, .high = 30});
    CHECK(peaks.at(0) == Bands{.low = 10, .mid = 200, .high = 30});
    peaks.store(0, {.low = 50, .mid = 100, .high = 30});
    CHECK(peaks.at(0) == Bands{.low = 50, .mid = 200, .high = 30});
    peaks.store(1, {.low = 255, .mid = 0, .high = 1});
    CHECK(peaks.at(1) == Bands{.low = 255, .mid = 0, .high = 1});
    peaks.store(2, {.low = 9, .mid = 9, .high = 9});
    CHECK(peaks.at(2) == Bands{});
    CHECK(peaks.size() == 2);
}

TEST_CASE("TrackPeaks: a kick's low tone is bass, a voice's tone the mids, a hi-hat's the highs")
{
    // A sine at 0.8 is 204 at full height
    const Bands kick = bandsOf(60.0);
    CHECK(kick.low > 140);
    CHECK(kick.mid < 15);
    CHECK(kick.high == 0);

    const Bands voice = bandsOf(1'000.0);
    CHECK(voice.mid > 140);
    CHECK(voice.low < 5);
    CHECK(voice.high < 15);

    // The one-pole filters take some of the very high tones: about half the height, yet clearly highs
    const Bands hihat = bandsOf(10'000.0);
    CHECK(hihat.high > 90);
    CHECK(hihat.mid < 20);
    CHECK(hihat.low == 0);
}

TEST_CASE("TrackPeaks: louder than full scale is full scale")
{
    TrackPeaks peaks{rate, rate};
    peaks.add(0, sine(40.0, rate, 4.0F));
    CHECK(peaks.at(150).low == 255);
}

TEST_CASE("TrackPeaks: blocks that follow each other give what one block gives, wherever they are cut")
{
    const auto whole = sine(300.0, 4 * per_peak);
    TrackPeaks once{4 * per_peak};
    once.add(0, whole);
    TrackPeaks parts{4 * per_peak};
    parts.add(0, std::span{whole}.first(100 * 2));
    parts.add(100, std::span{whole}.subspan(100 * 2, 300 * 2));
    parts.add(400, std::span{whole}.subspan(400 * 2));
    for (std::size_t index = 0; index < 4; ++index)
    {
        CAPTURE(index);
        CHECK(parts.at(index) == once.at(index));
    }
}

TEST_CASE("TrackPeaks: a block somewhere else starts the filters at its first sample, without a step")
{
    // A level that stands is bass only, also where the decoder jumped to it
    const std::vector<float> standing(2 * per_peak * 2, 0.5F);
    TrackPeaks               peaks{10 * per_peak};
    peaks.add(6 * per_peak, standing);
    CHECK(peaks.at(6) == Bands{.low = 128, .mid = 0, .high = 0});
    CHECK(peaks.at(7) == Bands{.low = 128, .mid = 0, .high = 0});
    CHECK(peaks.at(5) == Bands{});

    // The start of the track after that: filtered afresh as well
    peaks.add(0, standing);
    CHECK(peaks.at(0) == Bands{.low = 128, .mid = 0, .high = 0});
}

TEST_CASE("TrackPeaks: a block that ends inside a value writes what it has; frames beyond the track are ignored")
{
    TrackPeaks               peaks{per_peak};
    const std::vector<float> standing(10 * 2, 0.5F);
    peaks.add(per_peak - 2, standing);
    CHECK(peaks.at(0).low == 128);
    CHECK(peaks.at(1) == Bands{});
    CHECK(peaks.size() == 1);
}
