// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "timed_state.hpp"

#include <doctest/doctest.h>

using namespace djw::player_ui::sdl;
using namespace std::chrono_literals;

TEST_CASE("TimedNotice: shows its key for the duration, then nothing; a new key starts the time again")
{
    std::chrono::steady_clock::time_point now{};
    TimedNotice                           notice{[&now] { return now; }, 3'000ms};
    CHECK(notice.current().empty());

    notice.show("first");
    CHECK(notice.current() == "first");
    now += 2'999ms;
    CHECK(notice.current() == "first");
    now += 1ms;
    CHECK(notice.current().empty());

    notice.show("second");
    now += 2'000ms;
    CHECK(notice.current() == "second");
}

TEST_CASE("ConfirmTap: the second tap within the window confirms, a late one arms again")
{
    std::chrono::steady_clock::time_point now{};
    ConfirmTap                            tap{[&now] { return now; }, 3'000ms};
    CHECK_FALSE(tap.armed());

    CHECK_FALSE(tap.tap()); // arms
    CHECK(tap.armed());
    now += 3'000ms; // the last moment that counts
    CHECK(tap.tap());
    CHECK_FALSE(tap.armed()); // used up

    CHECK_FALSE(tap.tap());
    now += 3'001ms;
    CHECK_FALSE(tap.armed()); // timed out
    CHECK_FALSE(tap.tap());   // arms again instead of confirming
    CHECK(tap.armed());

    tap.cancel();
    CHECK_FALSE(tap.armed());
    CHECK_FALSE(tap.tap());
}
