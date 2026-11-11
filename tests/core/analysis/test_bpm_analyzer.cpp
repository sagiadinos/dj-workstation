// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "analysis/bpm_analyzer.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

using djw::core::analysis::BpmAnalyzer;

namespace
{
    constexpr std::uint32_t rate = 48'000;

    /// Interleaved stereo: a kick (decaying sine) on every beat
    std::vector<float> beatTrack(double bpm, double seconds)
    {
        const auto         frames = static_cast<std::size_t>(seconds * rate);
        std::vector<float> samples(frames * 2, 0.0F);
        const double       beat = 60.0 / bpm * rate;
        for (double start = 0.0; start < static_cast<double>(frames); start += beat)
        {
            for (std::size_t i = 0; i < 6'000 && static_cast<std::size_t>(start) + i < frames; ++i)
            {
                const double t = static_cast<double>(i) / rate;
                const auto   value =
                    static_cast<float>(0.8 * std::exp(-t * 40.0) * std::sin(2.0 * std::numbers::pi * 60.0 * t));
                samples[(static_cast<std::size_t>(start) + i) * 2] += value;
                samples[((static_cast<std::size_t>(start) + i) * 2) + 1] += value;
            }
        }
        return samples;
    }

    std::optional<double> analyze(const std::vector<float> &samples)
    {
        BpmAnalyzer           analyzer{rate};
        constexpr std::size_t block = 4'096 * 2;
        for (std::size_t at = 0; at < samples.size(); at += block)
        {
            analyzer.feed(std::span{samples}.subspan(at, std::min(block, samples.size() - at)));
        }
        return analyzer.finish();
    }
} // namespace

TEST_CASE("BpmAnalyzer: finds the tempo of a steady beat")
{
    for (const double bpm : {100.0, 140.0})
    {
        CAPTURE(bpm);
        const auto found = analyze(beatTrack(bpm, 24.0));
        REQUIRE(found.has_value());
        CHECK(*found == doctest::Approx(bpm).epsilon(0.01));
    }
}

TEST_CASE("BpmAnalyzer: a track that is too short or silent has no tempo")
{
    CHECK_FALSE(analyze(beatTrack(120.0, 3.0)).has_value());
    CHECK_FALSE(analyze(std::vector<float>(static_cast<std::size_t>(rate) * 12 * 2, 0.0F)).has_value());
}

TEST_CASE("BpmAnalyzer: the tempo is folded into the range of 70 to 180")
{
    const auto found = analyze(beatTrack(60.0, 24.0));
    REQUIRE(found.has_value());
    CHECK(*found >= BpmAnalyzer::min_bpm);
    CHECK(*found <= BpmAnalyzer::max_bpm);
}

TEST_CASE("BpmAnalyzer: just under the minimum length there is no tempo, even with enough beats")
{
    // 7.9 s at 160 BPM are 21 beats, plenty for the tracker; the track is still too short to trust
    CHECK_FALSE(analyze(beatTrack(160.0, 7.9)).has_value());
}

TEST_CASE("BpmAnalyzer: a fast tempo is folded down by halving, a slow one up by doubling, into the range")
{
    for (const double bpm : {200.0, 50.0})
    {
        CAPTURE(bpm);
        const auto found = analyze(beatTrack(bpm, 24.0));
        REQUIRE(found.has_value());
        CHECK(*found >= BpmAnalyzer::min_bpm);
        CHECK(*found <= BpmAnalyzer::max_bpm);
        // Folding keeps the beat: the tempo is the original one halved or doubled
        CHECK(*found == doctest::Approx(bpm > BpmAnalyzer::max_bpm ? bpm / 2.0 : bpm * 2.0).epsilon(0.02));
    }
}

TEST_CASE("BpmAnalyzer: folding halves or doubles until the tempo is in the range, the ends included")
{
    CHECK(BpmAnalyzer::foldIntoRange(120.0) == 120.0);
    CHECK(BpmAnalyzer::foldIntoRange(BpmAnalyzer::min_bpm) == BpmAnalyzer::min_bpm);
    CHECK(BpmAnalyzer::foldIntoRange(BpmAnalyzer::max_bpm) == BpmAnalyzer::max_bpm);
    CHECK(BpmAnalyzer::foldIntoRange(200.0) == 100.0);
    CHECK(BpmAnalyzer::foldIntoRange(500.0) == 125.0);
    CHECK(BpmAnalyzer::foldIntoRange(60.0) == 120.0);
    CHECK(BpmAnalyzer::foldIntoRange(20.0) == 80.0);
}
