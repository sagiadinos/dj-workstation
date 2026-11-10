// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "engine/headphone_bus.hpp"

#include <doctest/doctest.h>

#include <vector>

using djw::core::engine::HeadphoneBus;

namespace
{
    constexpr std::uint32_t sample_rate = 48'000;

    const std::vector<float> cue(4, 0.5F);     // 2 frames
    const std::vector<float> master(4, 0.25F); // 2 frames

    std::vector<float> process(HeadphoneBus &bus)
    {
        std::vector<float> output(4, 9.0F); // overwritten, not added to
        bus.process({.cue = cue, .master = master}, output);
        return output;
    }

    /// The second block, after the ramp to new settings
    std::vector<float> settled(HeadphoneBus &bus)
    {
        static_cast<void>(process(bus));
        return process(bus);
    }
} // namespace

TEST_CASE("HeadphoneBus: by default only the cue is heard")
{
    HeadphoneBus bus{sample_rate};
    CHECK(bus.cueMix() == 0.0F);
    CHECK(bus.volume() == 1.0F);
    CHECK(process(bus) == std::vector<float>(4, 0.5F));
}

TEST_CASE("HeadphoneBus: the cue mix blends linearly from cue to master")
{
    HeadphoneBus bus{sample_rate};

    bus.setCueMix(1.0F);
    CHECK(settled(bus) == std::vector<float>(4, 0.25F));

    bus.setCueMix(0.5F);
    CHECK(settled(bus) == std::vector<float>(4, 0.375F));
}

TEST_CASE("HeadphoneBus: the volume scales the headphone mix, ramped")
{
    HeadphoneBus bus{sample_rate};
    bus.setVolume(0.5F);
    CHECK(process(bus) == std::vector<float>{0.375F, 0.375F, 0.25F, 0.25F});
    CHECK(process(bus) == std::vector<float>(4, 0.25F));
}

TEST_CASE("HeadphoneBus: cue mix and volume are clamped to 0 ... 1")
{
    HeadphoneBus bus{sample_rate};
    bus.setCueMix(2.0F);
    bus.setVolume(-1.0F);
    CHECK(bus.cueMix() == 1.0F);
    CHECK(bus.volume() == 0.0F);
}

TEST_CASE("HeadphoneBus: the headphones are limited to full scale")
{
    HeadphoneBus             bus{sample_rate};
    const std::vector<float> loud(4, 3.0F);
    std::vector<float>       output(4);

    bus.process({.cue = loud, .master = master}, output);

    CHECK(output == std::vector<float>(4, 1.0F));
}
