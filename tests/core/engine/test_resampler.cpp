// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "engine/resampler.hpp"

#include "allocation_counter.hpp"
#include "test_sources.hpp"

#include <doctest/doctest.h>

#include <vector>

using djw::core::engine::AudioSource;
using djw::core::engine::Resampler;
using djw::tests::AllocationCounter;
using djw::tests::BufferSource;
using djw::tests::ramp;

namespace
{
    /// Hands out its frames in small portions and only up to a limit that the test can raise, like a track that
    /// is still being decoded
    class TrickleSource final : public AudioSource
    {
        public:
            explicit TrickleSource(std::size_t frames) : samples{ramp(frames)} {}

            std::size_t read(std::span<float> output) noexcept override
            {
                const std::size_t count = std::min({output.size(), available * 2 - position, std::size_t{6}}) / 2 * 2;
                std::copy_n(samples.begin() + static_cast<std::ptrdiff_t>(position), count, output.begin());
                position += count;
                return count / 2;
            }
            void seek(std::uint64_t frame) noexcept override { position = static_cast<std::size_t>(frame) * 2; }
            std::uint64_t lengthFrames() const noexcept override { return samples.size() / 2; }
            bool          atEnd() const noexcept override { return position >= samples.size(); }

            std::size_t available = 0;

        private:
            std::vector<float> samples;
            std::size_t        position = 0;
    };

    std::vector<float> process(Resampler &resampler, AudioSource &source, double speed, std::size_t frames)
    {
        std::vector<float> output(frames * 2, -1.0F);
        output.resize(resampler.process(source, speed, output) * 2);
        return output;
    }
} // namespace

TEST_CASE("Resampler: at speed 1 the output is the input, bit for bit")
{
    BufferSource source{ramp(5'000)};
    Resampler    resampler;

    const auto output = process(resampler, source, 1.0, 5'000);

    CHECK(output == ramp(5'000));
    CHECK(resampler.position() == 5'000.0);
}

TEST_CASE("Resampler: a ramp is reproduced exactly at any speed")
{
    // Catmull-Rom is exact for linear signals; only positions before frame 1 still see the silent history
    for (const double speed : {0.88, 0.96, 1.04, 1.12, 1.1648})
    {
        CAPTURE(speed);
        BufferSource source{ramp(10'000)};
        Resampler    resampler;

        const auto output = process(resampler, source, speed, 5'000);

        REQUIRE(output.size() == 5'000 * 2);
        for (std::size_t frame = 2; frame < 5'000; ++frame)
        {
            REQUIRE(output[frame * 2] == doctest::Approx(speed * static_cast<double>(frame)).epsilon(1e-6));
            REQUIRE(output[frame * 2 + 1] == output[frame * 2]);
        }
        CHECK(resampler.position() == doctest::Approx(speed * 5'000));
    }
}

TEST_CASE("Resampler: speed changes between calls continue from the exact position")
{
    BufferSource source{ramp(10'000)};
    Resampler    resampler;

    static_cast<void>(process(resampler, source, 1.0, 100));
    const auto faster = process(resampler, source, 1.5, 100);

    CHECK(faster[0] == doctest::Approx(100.0));
    CHECK(faster[2] == doctest::Approx(101.5));
    CHECK(resampler.position() == doctest::Approx(250.0));
}

TEST_CASE("Resampler: plays every frame of the source, then reports drained")
{
    BufferSource source{ramp(10)};
    Resampler    resampler;

    const auto output = process(resampler, source, 1.0, 50);

    CHECK(output == ramp(10));
    CHECK(resampler.drained());
    CHECK(process(resampler, source, 1.0, 10).empty());
}

TEST_CASE("Resampler: waits when the source has nothing yet and continues seamlessly")
{
    TrickleSource source{1'000};
    Resampler     resampler;
    source.available = 100;

    const auto first = process(resampler, source, 1.0, 500);
    CHECK(first.size() == 98 * 2); // two frames of lookahead are needed after the last one played
    CHECK_FALSE(resampler.drained());

    source.available = 1'000;
    const auto rest  = process(resampler, source, 1.0, 2'000);
    CHECK(rest.size() == 902 * 2);
    CHECK(rest[0] == 98.0F);
    CHECK(resampler.drained());
}

TEST_CASE("Resampler: reset starts over at the given frame")
{
    BufferSource source{ramp(1'000)};
    Resampler    resampler;
    static_cast<void>(process(resampler, source, 1.0, 300));

    source.seek(700);
    resampler.reset(700);
    const auto output = process(resampler, source, 1.0, 10);

    CHECK(output[0] == 700.0F);
    CHECK(output[18] == 709.0F);
    CHECK(resampler.position() == 710.0);
}

TEST_CASE("Resampler: process does not allocate or free memory")
{
    BufferSource       source{ramp(100'000)};
    Resampler          resampler;
    std::vector<float> output(256 * 2);

    const AllocationCounter counter;
    for (int i = 0; i < 100; ++i)
    {
        static_cast<void>(resampler.process(source, 1.0 + i * 0.001, output));
    }
    CHECK(counter.allocations() == 0);
    CHECK(counter.deallocations() == 0);
}
