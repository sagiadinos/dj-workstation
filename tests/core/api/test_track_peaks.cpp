// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "api/track_peaks.hpp"

#include <doctest/doctest.h>

#include <vector>

using djw::core::api::TrackPeaks;

namespace
{
    constexpr std::size_t per_peak = TrackPeaks::frames_per_peak;

    /// frames of interleaved stereo, all value except one frame at loud_frame with loud on the right channel
    std::vector<float> stereo(std::size_t frames, float value, std::size_t loud_frame = 0, float loud = 0.0F)
    {
        std::vector<float> samples(frames * 2, value);
        if (loud_frame < frames)
        {
            samples[(loud_frame * 2) + 1] = loud;
        }
        return samples;
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
        CHECK(peaks.at(index) == 0);
    }
    CHECK(peaks.at(4) == 0);
    CHECK(peaks.at(1'000'000) == 0);
}

TEST_CASE("TrackPeaks: each value is the loudest sample of its frames, either channel, either sign, 255 at full scale")
{
    TrackPeaks peaks{3 * per_peak};
    auto       samples              = stereo(3 * per_peak, 0.1F, per_peak + 7, -0.5F); // in the second value
    samples[(3 * per_peak * 2) - 2] = 1.0F;                                            // the last frame, left
    peaks.add(0, samples);
    CHECK(peaks.at(0) == 26); // 0.1 * 255 = 25.5, rounded
    CHECK(peaks.at(1) == 128);
    CHECK(peaks.at(2) == 255);
}

TEST_CASE("TrackPeaks: louder than full scale is full scale")
{
    TrackPeaks peaks{per_peak};
    peaks.add(0, stereo(per_peak, 0.0F, 3, 4.0F));
    CHECK(peaks.at(0) == 255);
}

TEST_CASE("TrackPeaks: blocks that do not start on a value add to the values they fall in; a quieter one keeps the "
          "louder")
{
    TrackPeaks peaks{3 * per_peak};
    // Frames 100 to 299: the end of value 0 and the start of value 1
    peaks.add(100, stereo(200, 0.2F));
    CHECK(peaks.at(0) == 51);
    CHECK(peaks.at(1) == 51);
    CHECK(peaks.at(2) == 0);
    // A louder frame at 300, in value 1, and a quieter block over value 0
    peaks.add(300, stereo(1, 0.6F));
    peaks.add(0, stereo(per_peak, 0.1F));
    CHECK(peaks.at(0) == 51);
    CHECK(peaks.at(1) == 153);
    // The frame just before the boundary belongs to value 0, the one on it to value 1
    TrackPeaks edge{2 * per_peak};
    edge.add(per_peak - 1, stereo(1, 0.4F));
    edge.add(per_peak, stereo(1, 0.8F));
    CHECK(edge.at(0) == 102);
    CHECK(edge.at(1) == 204);
}

TEST_CASE("TrackPeaks: frames beyond the track are ignored")
{
    TrackPeaks peaks{per_peak};
    peaks.add(per_peak - 2, stereo(10, 0.5F));
    CHECK(peaks.at(0) == 128);
    CHECK(peaks.at(1) == 0);
    peaks.add(5 * per_peak, stereo(4, 1.0F));
    CHECK(peaks.size() == 1);
}
