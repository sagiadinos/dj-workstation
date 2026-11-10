// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "engine/peak_meter.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <vector>

using djw::core::engine::PeakMeter;

namespace
{
    constexpr std::uint32_t sample_rate = 48'000;
}

TEST_CASE("PeakMeter: shows the largest absolute sample per channel at once")
{
    PeakMeter                meter{sample_rate};
    const std::vector<float> block{0.1F, -0.2F, -0.7F, 0.3F, 0.5F, 0.05F};

    meter.process(block);

    CHECK(meter.level().left == 0.7F);
    CHECK(meter.level().right == 0.3F);
}

TEST_CASE("PeakMeter: falls back by 20 dB in 1.7 s")
{
    PeakMeter meter{sample_rate};
    meter.process(std::vector<float>{1.0F, 0.5F});

    const std::vector<float> silence(static_cast<std::size_t>(1.7 * sample_rate) * 2, 0.0F);
    meter.process(silence);

    // One frame of the silence block is the peak frame itself, close enough
    CHECK(meter.level().left == doctest::Approx(0.1F).epsilon(1e-3));
    CHECK(meter.level().right == doctest::Approx(0.05F).epsilon(1e-3));
}

TEST_CASE("PeakMeter: the fall does not depend on the block size")
{
    PeakMeter whole{sample_rate};
    PeakMeter pieces{sample_rate};
    whole.process(std::vector<float>{0.8F, 0.8F});
    pieces.process(std::vector<float>{0.8F, 0.8F});

    whole.process(std::vector<float>(1'024 * 2, 0.0F));
    for (int i = 0; i < 4; ++i)
    {
        pieces.process(std::vector<float>(256 * 2, 0.0F));
    }

    CHECK(pieces.level().left == doctest::Approx(whole.level().left));
}

TEST_CASE("PeakMeter: a louder block replaces the falling level")
{
    PeakMeter meter{sample_rate};
    meter.process(std::vector<float>{0.2F, 0.2F});
    meter.process(std::vector<float>{0.9F, -0.6F});

    CHECK(meter.level().left == 0.9F);
    CHECK(meter.level().right == 0.6F);
}
