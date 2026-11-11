// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "layout.hpp"

#include <doctest/doctest.h>

using namespace djw::player_ui::sdl;
using djw::core::api::CrossfaderSide;

namespace
{
    Layout fiveWithMixer()
    {
        Layout layout;
        for (int index = 0; index < 5; ++index)
        {
            layout.players.push_back({{10 + (index * 400), 20, 380, 514},
                                      index == 2   ? CrossfaderSide::A
                                      : index == 1 ? CrossfaderSide::B
                                                   : CrossfaderSide::None});
        }
        layout.library  = {50, 600, 720, 640};
        layout.mixer    = {.open = true, .place = {900, 30, 440, 250}};
        layout.playlist = {.open = false, .place = {1300, 600, 560, 640}};
        return layout;
    }

    bool inside(const Placement &inner, const Placement &outer)
    {
        return inner.x >= outer.x && inner.y >= outer.y && inner.x + inner.width <= outer.x + outer.width &&
               inner.y + inner.height <= outer.y + outer.height;
    }
} // namespace

TEST_CASE("Layout: what is saved comes back the same, with any number of players")
{
    const Layout layout = fiveWithMixer();
    const auto   text   = formatLayout(layout);
    const auto   again  = parseLayout(text);

    REQUIRE(again.has_value());
    CHECK(*again == layout);
    CHECK(again->players.size() == 5);
    CHECK(again->players[2].side == CrossfaderSide::A); // "player 3 and player 2 to fade"
    CHECK(again->players[1].side == CrossfaderSide::B);
    CHECK(again->mixer.open);
    CHECK_FALSE(again->playlist.open);
}

TEST_CASE("Layout: windows may be at negative positions (a screen left of the main one)")
{
    Layout layout = fiveWithMixer();
    layout.players[0].place.x = -1'800;
    const auto again = parseLayout(formatLayout(layout));
    REQUIRE(again.has_value());
    CHECK(again->players[0].place.x == -1'800);
}

TEST_CASE("Layout: damaged or foreign text is no layout")
{
    const std::string good = formatLayout(fiveWithMixer());
    CHECK_FALSE(parseLayout("").has_value());
    CHECK_FALSE(parseLayout("something else\n").has_value());
    CHECK_FALSE(parseLayout("djw-layout 2\n" + good.substr(good.find('\n') + 1)).has_value());
    CHECK_FALSE(parseLayout(good + "unknown 1 2 3 4\n").has_value());
    CHECK_FALSE(parseLayout(good + "player A 0 0 50 50\n").has_value());     // too small to be a window
    CHECK_FALSE(parseLayout(good + "player X 0 0 380 514\n").has_value());   // no such side
    CHECK_FALSE(parseLayout(good + "player A 0 0 380 abc\n").has_value());   // not a number
    CHECK_FALSE(parseLayout("djw-layout 1\nlibrary 0 0 700 600\n").has_value()); // no player, no mixer
    // A missing line makes it incomplete
    std::string no_mixer = good.substr(0, good.find("mixer"));
    CHECK_FALSE(parseLayout(no_mixer).has_value());
}

TEST_CASE("Layout: more players than allowed is no layout")
{
    Layout layout = fiveWithMixer();
    layout.players.assign(max_layout_players + 1, layout.players.front());
    CHECK_FALSE(parseLayout(formatLayout(layout)).has_value());
    layout.players.pop_back();
    CHECK(parseLayout(formatLayout(layout)).has_value());
}

TEST_CASE("Layout: without a saved layout two players open with the crossfader window between them")
{
    const Placement    work{0, 0, 1920, 1040};
    const DefaultSizes sizes{380, 514, 440, 250, 720, 640};
    const Layout       layout = defaultLayout(work, sizes);

    REQUIRE(layout.players.size() == 2);
    CHECK(layout.mixer.open);
    CHECK_FALSE(layout.playlist.open);
    // Player 1, the crossfader window, player 2: left to right, the group centered
    CHECK(layout.players[0].place.x < layout.mixer.place.x);
    CHECK(layout.mixer.place.x < layout.players[1].place.x);
    CHECK(layout.players[0].place.x + layout.players[0].place.width <= layout.mixer.place.x);
    CHECK(layout.mixer.place.x + layout.mixer.place.width <= layout.players[1].place.x);
    const int left  = layout.players[0].place.x;
    const int right = work.width - (layout.players[1].place.x + layout.players[1].place.width);
    CHECK(std::abs(left - right) <= 1);
    // Player 1 is on side A, player 2 on side B, so the crossfader does something at once
    CHECK(layout.players[0].side == CrossfaderSide::A);
    CHECK(layout.players[1].side == CrossfaderSide::B);
    // The library below the players
    CHECK(layout.library.y >= layout.players[0].place.y + layout.players[0].place.height);
    CHECK(inside(layout.library, work));
}

TEST_CASE("Layout: on a small screen everything is moved inside the work area")
{
    const Placement    work{100, 50, 800, 600};
    const DefaultSizes sizes{380, 514, 440, 250, 720, 640};
    const Layout       layout = defaultLayout(work, sizes);
    for (const auto &player : layout.players)
    {
        CHECK(inside(player.place, work));
    }
    CHECK(inside(layout.mixer.place, work));
    CHECK(inside(layout.library, work));
}

TEST_CASE("fitInto: moves a window back into the area and shrinks one that is too big")
{
    const Placement area{0, 0, 1000, 700};
    CHECK(fitInto({2'000, 100, 380, 514}, area) == Placement{620, 100, 380, 514});
    CHECK(fitInto({-300, -50, 380, 514}, area) == Placement{0, 0, 380, 514});
    CHECK(fitInto({10, 10, 3'000, 2'000}, area) == Placement{0, 0, 1000, 700});
    CHECK(fitInto({10, 10, 380, 514}, area) == Placement{10, 10, 380, 514});
}
