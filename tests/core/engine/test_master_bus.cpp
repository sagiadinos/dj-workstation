// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "engine/master_bus.hpp"

#include <doctest/doctest.h>

#include <vector>

using djw::core::engine::MasterBus;

namespace
{
    constexpr std::uint32_t sample_rate = 48'000;
}

TEST_CASE("MasterBus: at full volume a signal below full scale passes unchanged")
{
    MasterBus                master{sample_rate};
    const std::vector<float> original{0.5F, -0.25F, 0.75F, 0.0F};
    std::vector<float>       samples = original;

    master.process(samples);

    CHECK(samples == original);
    CHECK(master.limiterGain() == 1.0F);
}

TEST_CASE("MasterBus: the volume ramps and is clamped to 0 ... 1")
{
    MasterBus master{sample_rate};
    master.setVolume(-1.0F);
    CHECK(master.volume() == 0.0F);
    master.setVolume(2.0F);
    CHECK(master.volume() == 1.0F);

    master.setVolume(0.5F);
    std::vector<float> samples(4, 0.8F);
    master.process(samples);
    CHECK(samples == std::vector<float>{0.6F, 0.6F, 0.4F, 0.4F});
}

TEST_CASE("MasterBus: the volume comes before the limiter, the meter after it")
{
    MasterBus master{sample_rate};
    master.setVolume(0.5F);
    std::vector<float> settle(2, 0.0F);
    master.process(settle);

    std::vector<float> samples{1.6F, -1.6F}; // 0.8 after the volume: no limiting
    master.process(samples);
    CHECK(samples == std::vector<float>{0.8F, -0.8F});
    CHECK(master.limiterGain() == 1.0F);

    master.setVolume(1.0F);
    master.process(settle);
    std::vector<float> loud{3.0F, 3.0F};
    master.process(loud);
    CHECK(loud[0] <= 1.0F);
    CHECK(master.level().left == loud[0]);
}
