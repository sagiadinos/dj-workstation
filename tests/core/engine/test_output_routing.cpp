// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "engine/output_routing.hpp"

#include <doctest/doctest.h>

#include <span>
#include <vector>

using djw::core::engine::routeOutput;

namespace
{
    const std::vector<float> master{1.0F, 2.0F, 3.0F, 4.0F};     // 2 frames
    const std::vector<float> headphones{5.0F, 6.0F, 7.0F, 8.0F}; // 2 frames
} // namespace

TEST_CASE("routeOutput: stereo carries the master only")
{
    std::vector<float> output(4, 9.0F);
    routeOutput({.master = master, .headphones = headphones}, output, 2);
    CHECK(output == master);
}

TEST_CASE("routeOutput: 4 channels carry the master on 1/2 and the headphones on 3/4")
{
    std::vector<float> output(8, 9.0F);
    routeOutput({.master = master, .headphones = headphones}, output, 4);
    CHECK(output == std::vector<float>{1.0F, 2.0F, 5.0F, 6.0F, 3.0F, 4.0F, 7.0F, 8.0F});
}

TEST_CASE("routeOutput: channels beyond the headphones are silent")
{
    std::vector<float> output(12, 9.0F);
    routeOutput({.master = master, .headphones = headphones}, output, 6);
    CHECK(output == std::vector<float>{1.0F, 2.0F, 5.0F, 6.0F, 0.0F, 0.0F, 3.0F, 4.0F, 7.0F, 8.0F, 0.0F, 0.0F});
}

TEST_CASE("routeOutput: 3 channels are too few for headphones, the third is silent")
{
    std::vector<float> output(6, 9.0F);
    routeOutput({.master = master, .headphones = headphones}, output, 3);
    CHECK(output == std::vector<float>{1.0F, 2.0F, 0.0F, 3.0F, 4.0F, 0.0F});
}

TEST_CASE("routeOutput: only as many frames as both sides have; the rest of the output is left alone")
{
    // More output than master: the third frame is not touched
    std::vector<float> longer(6, 9.0F);
    routeOutput({.master = master, .headphones = headphones}, longer, 2);
    CHECK(longer == std::vector<float>{1.0F, 2.0F, 3.0F, 4.0F, 9.0F, 9.0F});

    // Less output than master: one frame, nothing written beyond it
    std::vector<float>     buffer(8, 9.0F);
    const std::span<float> shorter = std::span{buffer}.first(4);
    routeOutput({.master = master, .headphones = headphones}, shorter, 4);
    CHECK(buffer == std::vector<float>{1.0F, 2.0F, 5.0F, 6.0F, 9.0F, 9.0F, 9.0F, 9.0F});
}
