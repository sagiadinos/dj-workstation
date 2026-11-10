// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "engine/limiter.hpp"

#include "test_sources.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <vector>

using djw::core::engine::Limiter;

namespace
{
    constexpr std::uint32_t sample_rate = 48'000;

    std::vector<float> sine(float amplitude, std::size_t frames)
    {
        djw::tests::SineSource source{440.0, sample_rate, amplitude};
        std::vector<float>     samples(frames * 2);
        static_cast<void>(source.read(samples));
        return samples;
    }

    float peak(const std::vector<float> &samples)
    {
        return std::ranges::max(samples, {}, [](float value) { return std::abs(value); });
    }
} // namespace

TEST_CASE("Limiter: a signal below full scale passes unchanged")
{
    Limiter                  limiter{sample_rate};
    const std::vector<float> original = sine(0.99F, 4'800);
    std::vector<float>       samples  = original;

    limiter.process(samples);

    CHECK(samples == original);
    CHECK(limiter.gain() == 1.0F);
}

TEST_CASE("Limiter: no sample leaves -1 ... +1, also at four times full scale")
{
    Limiter            limiter{sample_rate};
    std::vector<float> samples = sine(4.0F, 4'800);

    limiter.process(samples);

    CHECK(peak(samples) <= 1.0F);
    CHECK(limiter.gain() < 1.0F);
}

TEST_CASE("Limiter: limits instead of clipping, the waveform keeps its shape")
{
    Limiter            limiter{sample_rate};
    std::vector<float> loud = sine(2.0F, 48'000);
    limiter.process(loud);

    // After the first peak the gain holds near 0.5: the sine comes out at about full scale, not cut flat at 1.0 as
    // a clipper would do it (that would be off by up to 0.7 from the sine)
    const std::vector<float> settled{loud.end() - 4'800, loud.end()};
    const std::vector<float> reference = sine(1.0F, 48'000);
    const std::vector<float> reference_end{reference.end() - 4'800, reference.end()};
    for (std::size_t i = 0; i < settled.size(); ++i)
    {
        CHECK(settled[i] == doctest::Approx(reference_end[i]).epsilon(0.05).scale(1.0));
    }
}

TEST_CASE("Limiter: both channels share the gain")
{
    Limiter            limiter{sample_rate};
    std::vector<float> samples{2.0F, 0.5F};

    limiter.process(samples);

    CHECK(samples[0] == doctest::Approx(1.0F));
    CHECK(samples[1] == doctest::Approx(0.25F));
}

TEST_CASE("Limiter: the gain recovers once the signal is quiet again")
{
    Limiter            limiter{sample_rate};
    std::vector<float> loud{2.0F, 2.0F};
    limiter.process(loud);
    CHECK(limiter.gain() == doctest::Approx(0.5F));

    std::vector<float> quiet = sine(0.1F, 48'000); // 10 release time constants
    limiter.process(quiet);

    CHECK(limiter.gain() == doctest::Approx(1.0F).epsilon(1e-3));
}
