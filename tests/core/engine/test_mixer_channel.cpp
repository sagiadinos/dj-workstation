// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "engine/mixer_channel.hpp"

#include <doctest/doctest.h>

#include <vector>

using djw::core::api::CrossfaderSide;
using djw::core::engine::Crossfader;
using djw::core::engine::MixerChannel;

namespace
{
    constexpr std::uint32_t sample_rate = 48'000;

    /// Master and headphone cue output of one mixInto call
    struct Mixed
    {
            std::vector<float> master;
            std::vector<float> cue;
    };

    Mixed mix(MixerChannel &channel, const std::vector<float> &input, const Crossfader &crossfader = Crossfader{})
    {
        Mixed out{.master = std::vector<float>(input.size(), 0.0F), .cue = std::vector<float>(input.size(), 0.0F)};
        channel.mixInto(input, {.master = out.master, .cue = out.cue}, crossfader);
        return out;
    }
} // namespace

TEST_CASE("MixerChannel: full volume on no crossfader side adds the input unchanged")
{
    MixerChannel             channel{sample_rate};
    const std::vector<float> input{0.5F, -0.5F, 0.25F, -0.25F};
    std::vector<float>       master{0.1F, 0.1F, 0.1F, 0.1F};
    std::vector<float>       cue(4, 0.0F);

    channel.mixInto(input, {.master = master, .cue = cue}, Crossfader{});

    CHECK(master[0] == doctest::Approx(0.6F));
    CHECK(master[1] == doctest::Approx(-0.4F));
    CHECK(master[2] == doctest::Approx(0.35F));
    CHECK(master[3] == doctest::Approx(-0.15F));
}

TEST_CASE("MixerChannel: a volume change ramps over one block and then stays")
{
    MixerChannel             channel{sample_rate};
    const std::vector<float> input(8, 1.0F); // 4 frames

    channel.setVolume(0.0F);
    CHECK(mix(channel, input).master == std::vector<float>{0.75F, 0.75F, 0.5F, 0.5F, 0.25F, 0.25F, 0.0F, 0.0F});
    CHECK(mix(channel, input).master == std::vector<float>(8, 0.0F));
}

TEST_CASE("MixerChannel: volume and headphone level are clamped to 0 ... 1")
{
    MixerChannel channel{sample_rate};
    channel.setVolume(1.5F);
    CHECK(channel.volume() == 1.0F);
    channel.setVolume(-0.5F);
    CHECK(channel.volume() == 0.0F);
    channel.setHeadphoneLevel(2.0F);
    CHECK(channel.headphoneLevel() == 1.0F);
    channel.setHeadphoneLevel(-2.0F);
    CHECK(channel.headphoneLevel() == 0.0F);
}

TEST_CASE("MixerChannel: volume and crossfader gain multiply")
{
    MixerChannel channel{sample_rate};
    Crossfader   crossfader;
    channel.setVolume(0.5F);
    channel.setCrossfaderSide(CrossfaderSide::B);
    crossfader.setPosition(-1.0F);
    const std::vector<float> input(8, 1.0F);
    static_cast<void>(mix(channel, input, crossfader)); // ramps down to silent

    crossfader.setPosition(1.0F);
    CHECK(mix(channel, input, crossfader).master.back() == 0.5F);
    CHECK(mix(channel, input, crossfader).master == std::vector<float>(8, 0.5F));
    CHECK(channel.crossfaderSide() == CrossfaderSide::B);
}

TEST_CASE("MixerChannel: the meter shows the level before the fader")
{
    MixerChannel channel{sample_rate};
    channel.setVolume(0.0F);

    static_cast<void>(mix(channel, {0.6F, -0.4F}));

    CHECK(channel.level().left == 0.6F);
    CHECK(channel.level().right == 0.4F);
}

TEST_CASE("MixerChannel: nothing goes to the headphones without headphone cue")
{
    MixerChannel channel{sample_rate};
    CHECK_FALSE(channel.headphoneCue());
    CHECK(mix(channel, std::vector<float>(8, 0.5F)).cue == std::vector<float>(8, 0.0F));
}

TEST_CASE("MixerChannel: headphone cue takes the signal before fader and crossfader, at the headphone level")
{
    MixerChannel channel{sample_rate};
    Crossfader   crossfader;
    channel.setVolume(0.0F);
    channel.setCrossfaderSide(CrossfaderSide::A);
    crossfader.setPosition(1.0F);
    channel.setHeadphoneCue(true);
    channel.setHeadphoneLevel(0.5F);
    const std::vector<float> input(8, 1.0F);

    // Fades in over the first block like every fader
    CHECK(mix(channel, input, crossfader).cue ==
          std::vector<float>{0.125F, 0.125F, 0.25F, 0.25F, 0.375F, 0.375F, 0.5F, 0.5F});
    const Mixed steady = mix(channel, input, crossfader);
    CHECK(steady.cue == std::vector<float>(8, 0.5F));
    CHECK(steady.master == std::vector<float>(8, 0.0F));
    CHECK(channel.headphoneCue());

    channel.setHeadphoneCue(false);
    CHECK(mix(channel, input, crossfader).cue.back() == 0.0F);
}
