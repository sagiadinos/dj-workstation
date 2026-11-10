// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "engine/crossfader.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <initializer_list>

using djw::core::api::CrossfaderSide;
using djw::core::engine::Crossfader;

TEST_CASE("Crossfader: at the ends one side is full and the other exactly silent")
{
    Crossfader crossfader;

    crossfader.setPosition(-1.0F);
    CHECK(crossfader.gain(CrossfaderSide::A) == 1.0F);
    CHECK(crossfader.gain(CrossfaderSide::B) == 0.0F);

    crossfader.setPosition(1.0F);
    CHECK(crossfader.gain(CrossfaderSide::A) == 0.0F);
    CHECK(crossfader.gain(CrossfaderSide::B) == 1.0F);
}

TEST_CASE("Crossfader: constant power, both sides at -3 dB in the center")
{
    Crossfader crossfader;
    CHECK(crossfader.position() == 0.0F);
    CHECK(crossfader.gain(CrossfaderSide::A) == doctest::Approx(std::sqrt(0.5F)));
    CHECK(crossfader.gain(CrossfaderSide::B) == doctest::Approx(std::sqrt(0.5F)));

    for (const float position : {-0.8F, -0.3F, 0.1F, 0.6F})
    {
        crossfader.setPosition(position);
        const float a = crossfader.gain(CrossfaderSide::A);
        const float b = crossfader.gain(CrossfaderSide::B);
        CHECK((a * a) + (b * b) == doctest::Approx(1.0F));
    }
}

TEST_CASE("Crossfader: moving towards B lowers A and raises B")
{
    Crossfader crossfader;
    crossfader.setPosition(-0.5F);
    const float a_before = crossfader.gain(CrossfaderSide::A);
    const float b_before = crossfader.gain(CrossfaderSide::B);

    crossfader.setPosition(0.5F);

    CHECK(crossfader.gain(CrossfaderSide::A) < a_before);
    CHECK(crossfader.gain(CrossfaderSide::B) > b_before);
}

TEST_CASE("Crossfader: decks on no side are not affected")
{
    Crossfader crossfader;
    for (const float position : {-1.0F, 0.0F, 1.0F})
    {
        crossfader.setPosition(position);
        CHECK(crossfader.gain(CrossfaderSide::None) == 1.0F);
    }
}

TEST_CASE("Crossfader: the position is clamped to -1 ... +1")
{
    Crossfader crossfader;
    crossfader.setPosition(3.0F);
    CHECK(crossfader.position() == 1.0F);
    crossfader.setPosition(-3.0F);
    CHECK(crossfader.position() == -1.0F);
}

TEST_CASE("Crossfader: momentary A/B jumps to the side while held and back on release")
{
    Crossfader crossfader;
    crossfader.setPosition(0.4F);
    const float a_at_position = crossfader.gain(CrossfaderSide::A);

    crossfader.setMomentary(CrossfaderSide::A);
    CHECK(crossfader.gain(CrossfaderSide::A) == 1.0F);
    CHECK(crossfader.gain(CrossfaderSide::B) == 0.0F);
    CHECK(crossfader.position() == 0.4F); // the fader itself stays
    CHECK(crossfader.momentary() == CrossfaderSide::A);

    crossfader.setMomentary(CrossfaderSide::B);
    CHECK(crossfader.gain(CrossfaderSide::A) == 0.0F);
    CHECK(crossfader.gain(CrossfaderSide::B) == 1.0F);

    crossfader.setMomentary(CrossfaderSide::None);
    CHECK(crossfader.gain(CrossfaderSide::A) == a_at_position);
}
