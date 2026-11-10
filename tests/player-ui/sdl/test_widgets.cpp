// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "fader.hpp"
#include "play_button.hpp"
#include "time_format.hpp"

#include <doctest/doctest.h>

using namespace djw::player_ui::sdl;

namespace
{
    // Bounds 0..124 in y; the knob is 24 high, so the value maps to y = 12 (1.0) ... 112 (0.0)
    constexpr Rect fader_bounds{0, 0, 40, 124};
} // namespace

TEST_CASE("Fader: the value follows the finger position, top is 1, bottom is 0")
{
    Fader fader;
    fader.setBounds(fader_bounds);

    fader.pointerDown({20, 62});
    CHECK(fader.value() == doctest::Approx(0.5F));
    fader.pointerMove({20, 12});
    CHECK(fader.value() == doctest::Approx(1.0F));
    fader.pointerMove({20, 112});
    CHECK(fader.value() == doctest::Approx(0.0F));
}

TEST_CASE("Fader: outside the track the value stops at the ends")
{
    Fader fader;
    fader.setBounds(fader_bounds);

    fader.pointerDown({20, 62});
    fader.pointerMove({300, -500});
    CHECK(fader.value() == 1.0F);
    fader.pointerMove({300, 900});
    CHECK(fader.value() == 0.0F);
}

TEST_CASE("Fader: a fader with no room does not change its value")
{
    Fader fader;
    fader.setBounds({0, 0, 40, 10});
    fader.setValue(0.3F);

    fader.pointerDown({20, 5});

    CHECK(fader.value() == doctest::Approx(0.3F));
}

TEST_CASE("PlayButton: one press is reported exactly once")
{
    PlayButton button;
    CHECK_FALSE(button.takePress());

    button.pointerDown({1, 1});
    CHECK(button.takePress());
    CHECK_FALSE(button.takePress());

    button.pointerUp({1, 1});
    CHECK_FALSE(button.takePress());
}

TEST_CASE("formatDuration: minutes and seconds, rounded")
{
    CHECK(formatDuration(0.0) == "0:00");
    CHECK(formatDuration(4.4) == "0:04");
    CHECK(formatDuration(59.6) == "1:00");
    CHECK(formatDuration(245.0) == "4:05");
    CHECK(formatDuration(-3.0) == "0:00");
}

TEST_CASE("formatPosition: minutes, seconds and tenths, truncated")
{
    CHECK(formatPosition(0.0) == "0:00.0");
    CHECK(formatPosition(7.49) == "0:07.4");
    CHECK(formatPosition(59.99) == "0:59.9");
    CHECK(formatPosition(60.0) == "1:00.0");
    CHECK(formatPosition(187.4) == "3:07.4");
    CHECK(formatPosition(-1.0) == "0:00.0");
}
