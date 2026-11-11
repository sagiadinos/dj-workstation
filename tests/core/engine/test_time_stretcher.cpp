// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "engine/time_stretcher.hpp"

#include "allocation_counter.hpp"
#include "signal_analysis.hpp"
#include "test_sources.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <numbers>
#include <vector>

using djw::core::engine::TimeStretcher;
using djw::tests::AllocationCounter;
using djw::tests::BufferSource;
using djw::tests::SineSource;

namespace
{
    constexpr std::uint32_t sample_rate = 48'000;

    double frequencyOf(const std::vector<float> &stereo)
    {
        return djw::tests::frequencyOf(stereo, sample_rate);
    }

    double rms(const std::vector<float> &stereo)
    {
        return djw::tests::rmsOf(stereo);
    }

    /// Renders frames of a 440 Hz sine through the stretcher in blocks and returns the part after the start-up
    std::vector<float> stretchedSine(double tempo, std::size_t frames)
    {
        SineSource         source{440.0, sample_rate, 0.5F};
        TimeStretcher      stretcher{sample_rate};
        std::vector<float> all;
        std::vector<float> block(480 * 2);
        while (all.size() < frames * 2)
        {
            const std::size_t made = stretcher.process(source, tempo, block);
            REQUIRE(made == 480);
            all.insert(all.end(), block.begin(), block.end());
        }
        return {all.begin() + 4'800 * 2, all.end()}; // the first 0.1 s are the start-up of the algorithm
    }

    std::vector<float> sineBuffer(std::size_t frames)
    {
        std::vector<float> samples(frames * 2);
        for (std::size_t frame = 0; frame < frames; ++frame)
        {
            const auto value = static_cast<float>(
                0.5 * std::sin(2.0 * std::numbers::pi * 440.0 * static_cast<double>(frame) / sample_rate));
            samples[frame * 2]     = value;
            samples[frame * 2 + 1] = value;
        }
        return samples;
    }
} // namespace

TEST_CASE("TimeStretcher: at tempo 1.0 the sine keeps its pitch and level")
{
    const auto output = stretchedSine(1.0, 48'000);
    CHECK(frequencyOf(output) == doctest::Approx(440.0).epsilon(0.01));
    CHECK(rms(output) == doctest::Approx(0.5 / std::numbers::sqrt2).epsilon(0.05));
}

TEST_CASE("TimeStretcher: a faster tempo keeps the pitch")
{
    for (const double tempo : {1.08, 1.16, 1.25})
    {
        CAPTURE(tempo);
        const auto output = stretchedSine(tempo, 48'000);
        CHECK(frequencyOf(output) == doctest::Approx(440.0).epsilon(0.02));
        CHECK(rms(output) == doctest::Approx(0.5 / std::numbers::sqrt2).epsilon(0.1));
    }
}

TEST_CASE("TimeStretcher: a slower tempo keeps the pitch")
{
    for (const double tempo : {0.8, 0.9, 0.95})
    {
        CAPTURE(tempo);
        const auto output = stretchedSine(tempo, 48'000);
        CHECK(frequencyOf(output) == doctest::Approx(440.0).epsilon(0.02));
        CHECK(rms(output) == doctest::Approx(0.5 / std::numbers::sqrt2).epsilon(0.1));
    }
}

TEST_CASE("TimeStretcher: the position moves at the tempo")
{
    SineSource         source{440.0, sample_rate, 0.5F};
    TimeStretcher      stretcher{sample_rate};
    std::vector<float> block(480 * 2);
    stretcher.reset(10'000);
    CHECK(stretcher.position() == 10'000);

    for (int i = 0; i < 100; ++i) // 1 s of output
    {
        REQUIRE(stretcher.process(source, 1.25, block) == 480);
    }
    // 1 s at tempo 1.25 is 1.25 s of the track, less what SoundTouch holds back (a few 10 ms)
    const double moved = stretcher.position() - 10'000;
    CHECK(moved > 1.25 * sample_rate - 3'000);
    CHECK(moved < 1.25 * sample_rate + 500);
}

TEST_CASE("TimeStretcher: reset moves the position and forgets what was held")
{
    SineSource         source{440.0, sample_rate, 0.5F};
    TimeStretcher      stretcher{sample_rate};
    std::vector<float> block(480 * 2);
    for (int i = 0; i < 20; ++i)
    {
        static_cast<void>(stretcher.process(source, 1.1, block));
    }
    stretcher.reset(123'456);
    CHECK(stretcher.position() == 123'456);
    CHECK_FALSE(stretcher.drained());
}

TEST_CASE("TimeStretcher: plays a track to its end and then is drained, about length / tempo long")
{
    constexpr std::size_t length = 48'000;
    BufferSource          source{sineBuffer(length)};
    TimeStretcher         stretcher{sample_rate};
    std::vector<float>    block(480 * 2);

    std::size_t total = 0;
    for (int i = 0; i < 1'000 && !stretcher.drained(); ++i)
    {
        total += stretcher.process(source, 1.25, block);
    }
    CHECK(stretcher.drained());
    const double expected = length / 1.25;
    CHECK(static_cast<double>(total) > expected - 3'000);
    CHECK(static_cast<double>(total) < expected + 3'000);
}

TEST_CASE("TimeStretcher: the position counts what is still held back at the tempo, so it ends at the length")
{
    constexpr std::size_t length = 96'000;
    constexpr double      tempo  = 1.25;
    BufferSource          source{sineBuffer(length)};
    TimeStretcher         stretcher{sample_rate};
    std::vector<float>    block(480 * 2);

    // Half way: the position plus what is still to be played (at the tempo) is the length
    std::size_t played = 0;
    while (played < 30'000)
    {
        played += stretcher.process(source, tempo, block);
    }
    const double half_way = stretcher.position();
    std::size_t  rest     = 0;
    for (int i = 0; i < 1'000 && !stretcher.drained(); ++i)
    {
        rest += stretcher.process(source, tempo, block);
    }
    REQUIRE(stretcher.drained());
    CHECK(half_way + (static_cast<double>(rest) * tempo) == doctest::Approx(length).epsilon(0.005));
    CHECK(stretcher.position() == doctest::Approx(length).epsilon(0.001));
}

TEST_CASE("TimeStretcher: a source that has nothing yet gives fewer frames, not an error")
{
    class EmptySource final : public djw::core::engine::AudioSource
    {
        public:
            std::size_t   read(std::span<float>) noexcept override { return 0; }
            void          seek(std::uint64_t) noexcept override {}
            std::uint64_t lengthFrames() const noexcept override { return 1'000'000; }
            bool          atEnd() const noexcept override { return false; }
    };
    EmptySource        source;
    TimeStretcher      stretcher{sample_rate};
    std::vector<float> block(480 * 2);
    CHECK(stretcher.process(source, 1.0, block) == 0);
    CHECK_FALSE(stretcher.drained());
}

TEST_CASE("TimeStretcher: process does not allocate or free memory, also while the tempo changes")
{
    SineSource         source{440.0, sample_rate, 0.5F};
    TimeStretcher      stretcher{sample_rate};
    std::vector<float> block(1'024 * 2);

    const AllocationCounter counter;
    for (int i = 0; i < 400; ++i)
    {
        const double tempo = 0.82 + (0.4 * (i % 50) / 50.0); // the pitch range and the bend buttons
        static_cast<void>(stretcher.process(source, tempo, block));
    }
    stretcher.reset(0);
    for (int i = 0; i < 50; ++i)
    {
        static_cast<void>(stretcher.process(source, 1.0, block));
    }
    CHECK(counter.allocations() == 0);
    CHECK(counter.deallocations() == 0);
}
