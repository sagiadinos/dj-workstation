// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "engine/gain_ramp.hpp"

#include <doctest/doctest.h>

#include <vector>

using djw::core::engine::GainRamp;

TEST_CASE("GainRamp: at gain 1.0 addTo adds the input unchanged")
{
    GainRamp                 ramp;
    const std::vector<float> input{0.5F, -0.5F, 0.25F, -0.25F};
    std::vector<float>       output{0.1F, 0.1F, 0.1F, 0.1F};

    ramp.addTo(input, output);

    CHECK(output[0] == doctest::Approx(0.6F));
    CHECK(output[1] == doctest::Approx(-0.4F));
    CHECK(output[2] == doctest::Approx(0.35F));
    CHECK(output[3] == doctest::Approx(-0.15F));
}

TEST_CASE("GainRamp: a new target is reached linearly on the last frame of the next block and then stays")
{
    GainRamp                 ramp;
    const std::vector<float> input(8, 1.0F); // 4 frames
    std::vector<float>       output(8, 0.0F);

    ramp.setTarget(0.0F);
    ramp.addTo(input, output);
    CHECK(output == std::vector<float>{0.75F, 0.75F, 0.5F, 0.5F, 0.25F, 0.25F, 0.0F, 0.0F});

    std::vector<float> next(8, 0.0F);
    ramp.addTo(input, next);
    CHECK(next == std::vector<float>(8, 0.0F));
}

TEST_CASE("GainRamp: applyTo scales in place with the same ramp")
{
    GainRamp           ramp{0.0F};
    std::vector<float> samples(8, 2.0F);

    ramp.setTarget(1.0F);
    ramp.applyTo(samples);

    CHECK(samples == std::vector<float>{0.5F, 0.5F, 1.0F, 1.0F, 1.5F, 1.5F, 2.0F, 2.0F});
    CHECK(ramp.target() == 1.0F);
}

TEST_CASE("GainRamp: an empty block keeps the ramp pending")
{
    GainRamp           ramp;
    std::vector<float> empty;
    ramp.setTarget(0.0F);
    ramp.applyTo(empty);

    std::vector<float> samples(4, 1.0F);
    ramp.applyTo(samples);
    CHECK(samples == std::vector<float>{0.5F, 0.5F, 0.0F, 0.0F});
}
