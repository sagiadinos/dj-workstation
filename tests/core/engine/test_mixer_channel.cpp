// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "engine/mixer_channel.hpp"

#include <doctest/doctest.h>

#include <vector>

using djw::core::engine::MixerChannel;

TEST_CASE("MixerChannel: full volume adds the input unchanged")
{
    MixerChannel             channel;
    const std::vector<float> input{0.5F, -0.5F, 0.25F, -0.25F};
    std::vector<float>       output{0.1F, 0.1F, 0.1F, 0.1F};

    channel.mixInto(input, output);

    CHECK(output[0] == doctest::Approx(0.6F));
    CHECK(output[1] == doctest::Approx(-0.4F));
    CHECK(output[2] == doctest::Approx(0.35F));
    CHECK(output[3] == doctest::Approx(-0.15F));
}

TEST_CASE("MixerChannel: a volume change ramps over one block and then stays")
{
    MixerChannel             channel;
    const std::vector<float> input(8, 1.0F); // 4 frames
    std::vector<float>       output(8, 0.0F);

    channel.setVolume(0.0F);
    channel.mixInto(input, output);
    // Linear from 1.0 towards 0.0, reaching it on the last frame
    CHECK(output[0] == doctest::Approx(0.75F));
    CHECK(output[2] == doctest::Approx(0.5F));
    CHECK(output[4] == doctest::Approx(0.25F));
    CHECK(output[6] == doctest::Approx(0.0F));
    CHECK(output[1] == output[0]);

    std::vector<float> next(8, 0.0F);
    channel.mixInto(input, next);
    CHECK(next == std::vector<float>(8, 0.0F));
}

TEST_CASE("MixerChannel: volume is clamped to 0 ... 1")
{
    MixerChannel channel;
    channel.setVolume(1.5F);
    CHECK(channel.volume() == 1.0F);
    channel.setVolume(-0.5F);
    CHECK(channel.volume() == 0.0F);
}
