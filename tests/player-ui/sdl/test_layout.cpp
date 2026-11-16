// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "layout.hpp"

#include <doctest/doctest.h>

#include <string>
#include <string_view>

using namespace djw::player_ui::sdl;
using djw::core::api::CrossfaderSide;
namespace api = djw::core::api;

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
    Layout layout             = fiveWithMixer();
    layout.players[0].place.x = -1'800;
    const auto again          = parseLayout(formatLayout(layout));
    REQUIRE(again.has_value());
    CHECK(again->players[0].place.x == -1'800);
}

TEST_CASE("Layout: damaged or foreign text is no layout")
{
    const std::string good = formatLayout(fiveWithMixer());
    CHECK_FALSE(parseLayout("").has_value());
    CHECK_FALSE(parseLayout("something else\n").has_value());
    CHECK_FALSE(parseLayout("djw-layout 3\n" + good.substr(good.find('\n') + 1)).has_value());
    CHECK_FALSE(parseLayout(good + "unknown 1 2 3 4\n").has_value());
    CHECK_FALSE(parseLayout(good + "player A 0 0 50 50\n").has_value());   // too small to be a window
    CHECK_FALSE(parseLayout(good + "player X 0 0 380 514\n").has_value()); // no such side
    CHECK_FALSE(parseLayout(good + "player A 0 0 380 abc\n").has_value()); // not a number
    CHECK_FALSE(parseLayout("djw-layout 1\n" + good.substr(good.find('\n') + 1)).has_value()); // other version
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

TEST_CASE("Layout: the window mode and the layout of the one window come back as saved")
{
    Layout layout = fiveWithMixer();
    CHECK(parseLayout(formatLayout(layout))->mode == WindowMode::Separate);
    CHECK_FALSE(parseLayout(formatLayout(layout))->mdi.has_value());

    layout.mode      = WindowMode::Mdi;
    layout.mdi       = MdiLayout{.players = {api::CrossfaderSide::B, api::CrossfaderSide::None, api::CrossfaderSide::A},
                                 // Each player's Time and Cue-Load: on, off, one of them
                                 .player_options = {{.show_remaining = true, .cue_load = true}, {}, {.cue_load = true}},
                                 .mixer_open     = false,
                                 .playlist_open  = true,
                                 .fullscreen     = true,
                                 .places         = std::nullopt};
    const auto again = parseLayout(formatLayout(layout));
    REQUIRE(again.has_value());
    CHECK(*again == layout);

    // The one window without players is fine: only the library
    layout.mdi->players.clear();
    layout.mdi->player_options.clear();
    CHECK(*parseLayout(formatLayout(layout)) == layout);
}

TEST_CASE("Layout: each player keeps its Time and Cue-Load options; a line from before them has both off")
{
    Layout layout                  = fiveWithMixer();
    layout.players.front().options = {.show_remaining = true, .cue_load = true};
    const std::string text         = formatLayout(layout);
    CHECK(text.find("remaining cue-load") != std::string::npos);
    const auto again = parseLayout(text);
    REQUIRE(again.has_value());
    CHECK(again->players.front().options == layout.players.front().options);
    CHECK(again->players.back().options == PlayerOptions{});

    // An option twice is no layout
    std::string twice = text;
    twice.insert(twice.find("remaining") + std::string_view{"remaining"}.size(), " remaining");
    CHECK_FALSE(parseLayout(twice).has_value());
}

TEST_CASE("Layout: a damaged mode or one-window part is no layout")
{
    Layout layout          = fiveWithMixer();
    layout.mdi             = defaultMdiLayout();
    const std::string good = formatLayout(layout);
    REQUIRE(parseLayout(good).has_value());
    CHECK_FALSE(parseLayout(good + "mode windows\n").has_value());     // no such mode
    CHECK_FALSE(parseLayout(good + "mode mdi\n").has_value());         // twice
    CHECK_FALSE(parseLayout(good + "mdi player X\n").has_value());     // no such side
    CHECK_FALSE(parseLayout(good + "mdi mixer maybe\n").has_value());  // neither open nor closed
    CHECK_FALSE(parseLayout(good + "mdi library open\n").has_value()); // the library is always there
    CHECK_FALSE(parseLayout(good + "mdi player\n").has_value());
    // Without the mode line
    std::string no_mode = good;
    no_mode.erase(no_mode.find("mode"), no_mode.find('\n', no_mode.find("mode")) - no_mode.find("mode") + 1);
    CHECK_FALSE(parseLayout(no_mode).has_value());
    // The one window needs both of its tool lines
    std::string no_mdi_playlist = good.substr(0, good.find("mdi playlist"));
    CHECK_FALSE(parseLayout(no_mdi_playlist).has_value());
    // Too many players
    Layout crowded = layout;
    crowded.mdi->players.assign(max_layout_players + 1, api::CrossfaderSide::A);
    CHECK_FALSE(parseLayout(formatLayout(crowded)).has_value());
}

TEST_CASE(
    "Layout: without a saved one window two players on the sides A and B and the crossfader are shown, in a window")
{
    const MdiLayout mdi = defaultMdiLayout();
    CHECK(mdi.players == std::vector{api::CrossfaderSide::A, api::CrossfaderSide::B});
    CHECK(mdi.mixer_open);
    CHECK_FALSE(mdi.playlist_open);
    CHECK_FALSE(mdi.fullscreen);
    CHECK_FALSE(mdi.maximized);
    CHECK(mdi.window_width == 0); // the default size
}

TEST_CASE("Layout: the places of the one window set by hand come back as saved")
{
    Layout layout          = fiveWithMixer();
    layout.mode            = WindowMode::Mdi;
    layout.mdi             = MdiLayout{.players        = {api::CrossfaderSide::A, api::CrossfaderSide::B},
                                       .player_options = {{}, {.show_remaining = true}},
                                       .mixer_open     = false,
                                       .playlist_open  = true,
                                       .fullscreen     = false,
                                       .places         = MdiPlaces{.area     = Placement{0, 0, 1920, 1080},
                                                                   .players  = {Placement{0, 0, 380, 540}, std::nullopt},
                                                                   .mixer    = Placement{400, 0, 440, 280},
                                                                   .playlist = std::nullopt,
                                                                   .beats    = std::nullopt,
                                                                   .library  = Placement{0, 560, 1000, 400}}};
    const std::string text = formatLayout(layout);
    CHECK(text.find("mdi player A 0 0 380 540\n") != std::string::npos);
    CHECK(text.find("mdi player B remaining\n") != std::string::npos);         // the Time option after the side
    CHECK(text.find("mdi mixer closed 400 0 440 280\n") != std::string::npos); // closed, the place stays
    CHECK(text.find("mdi library 0 560 1000 400\n") != std::string::npos);
    CHECK(text.find("mdi area 1920 1080\n") != std::string::npos);
    CHECK(text.find("mdi fullscreen off\n") != std::string::npos);
    const auto again = parseLayout(text);
    REQUIRE(again.has_value());
    CHECK(*again == layout);
}

TEST_CASE("Layout: the one window comes back maximized or as a window of its size; before that line maximized")
{
    Layout layout          = fiveWithMixer();
    layout.mode            = WindowMode::Mdi;
    layout.mdi             = defaultMdiLayout();
    layout.mdi->fullscreen = false;

    layout.mdi->maximized = true;
    std::string text      = formatLayout(layout);
    CHECK(text.find("mdi window maximized\n") != std::string::npos);
    REQUIRE(parseLayout(text).has_value());
    CHECK(parseLayout(text)->mdi == layout.mdi);

    layout.mdi->maximized     = false;
    layout.mdi->window_width  = 1224;
    layout.mdi->window_height = 928;
    text                      = formatLayout(layout);
    CHECK(text.find("mdi window 1224 928\n") != std::string::npos);
    REQUIRE(parseLayout(text).has_value());
    CHECK(parseLayout(text)->mdi == layout.mdi);

    CHECK_FALSE(parseLayout(text + "mdi window maximized\n").has_value()); // twice
    CHECK_FALSE(parseLayout(text + "mdi window 1224\n").has_value());
    std::string without = text;
    without.erase(without.find("mdi window 1224 928\n"), std::string_view{"mdi window 1224 928\n"}.size());
    REQUIRE(parseLayout(without).has_value());
    CHECK(parseLayout(without)->mdi->maximized);
}

TEST_CASE("Layout: places of the one window without the library's, or damaged ones, are no layout")
{
    Layout layout          = fiveWithMixer();
    layout.mdi             = defaultMdiLayout();
    layout.mdi->fullscreen = true; // the line removed below
    const std::string good = formatLayout(layout);
    const std::size_t at   = good.find("mdi player A");
    REQUIRE(at != std::string::npos);
    std::string with_place = good;
    with_place.insert(at + std::string_view{"mdi player A"}.size(), " 0 0 380 540");
    CHECK_FALSE(parseLayout(with_place).has_value()); // no library place
    const std::string area = "mdi area 1920 1080\n";
    CHECK(parseLayout(with_place + "mdi library 0 560 1000 400\n" + area).has_value()); // with it and the size
    CHECK_FALSE(parseLayout(with_place + "mdi library 0 560 1000 400\n").has_value());  // without the size
    CHECK_FALSE(parseLayout(good + area).has_value());                                  // a size without places
    CHECK_FALSE(parseLayout(good + "mdi area 1920\n").has_value());
    CHECK_FALSE(parseLayout(good + "mdi fullscreen maybe\n").has_value());
    CHECK_FALSE(parseLayout(good + "mdi fullscreen on\n").has_value()); // twice: formatLayout wrote it already
    // A layout from before the fullscreen line opens in fullscreen
    std::string without_screen = good;
    without_screen.erase(without_screen.find("mdi fullscreen on\n"), std::string_view{"mdi fullscreen on\n"}.size());
    REQUIRE(parseLayout(without_screen).has_value());
    CHECK(parseLayout(without_screen)->mdi->fullscreen);
    CHECK_FALSE(parseLayout(good + "mdi library 0 560 1000\n").has_value());                           // three numbers
    CHECK_FALSE(parseLayout(good + "mdi library 0 560 10 400\n").has_value());                         // too small
    CHECK_FALSE(parseLayout(good + "mdi library 0 0 500 500\nmdi library 0 0 500 500\n").has_value()); // twice
    CHECK_FALSE(parseLayout(good + "mdi mixer open 1 2\n").has_value());
}

namespace
{
    /// A separate-windows layout as text, with the player line given and the other lines as given or plain
    std::string separateText(std::string_view player, std::string_view library = "library 0 600 720 640",
                             std::string_view mixer    = "mixer open 900 30 440 250",
                             std::string_view playlist = "playlist closed 1300 600 560 640",
                             std::string_view beats    = "beats closed 0 0 760 200")
    {
        std::string text = "djw-layout 2\nmode separate\n";
        for (const std::string_view line : {player, library, mixer, playlist, beats})
        {
            if (!line.empty())
            {
                text += std::string{line} + '\n';
            }
        }
        return text;
    }
} // namespace

TEST_CASE("Layout: sizes from 100 to 20000 and positions up to four times that; whole numbers only")
{
    CHECK(parseLayout(separateText("player A 0 0 100 100")).has_value());
    CHECK(parseLayout(separateText("player A 0 0 20000 20000")).has_value());
    CHECK(parseLayout(separateText("player A 80000 -80000 380 514")).has_value());
    CHECK_FALSE(parseLayout(separateText("player A 0 0 99 514")).has_value());
    CHECK_FALSE(parseLayout(separateText("player A 0 0 380 99")).has_value());
    CHECK_FALSE(parseLayout(separateText("player A 0 0 20001 514")).has_value());
    CHECK_FALSE(parseLayout(separateText("player A 0 0 380 20001")).has_value());
    CHECK_FALSE(parseLayout(separateText("player A 80001 0 380 514")).has_value());
    CHECK_FALSE(parseLayout(separateText("player A 0 -80001 380 514")).has_value());
    CHECK_FALSE(parseLayout(separateText("player A 0 0 380x 514")).has_value());
    CHECK_FALSE(parseLayout(separateText("player A x 0 380 514")).has_value());
    CHECK_FALSE(parseLayout(separateText("player A 0 y 380 514")).has_value());
    CHECK_FALSE(parseLayout(separateText("player A 0 0 w 514")).has_value());
    CHECK_FALSE(parseLayout(separateText("player A 0 0 380 h")).has_value());
    // The numbers read as written
    const auto read = parseLayout(separateText("player A -12 34 567 890"));
    REQUIRE(read.has_value());
    CHECK(read->players.front().place == Placement{-12, 34, 567, 890});
}

TEST_CASE("Layout: several blanks between the words and blanks at the end change nothing")
{
    const auto read = parseLayout(separateText("player  A   10 20 380 514  ", "library 0 600 720 640 "));
    REQUIRE(read.has_value());
    CHECK(read->players.front().place == Placement{10, 20, 380, 514});
    CHECK(read->library == Placement{0, 600, 720, 640});
}

TEST_CASE("Layout: without a player, the library, the crossfader, the playlist or the beats line it is no layout; nor"
          " with a line it does not know")
{
    CHECK(parseLayout(separateText("player A 0 0 380 514")).has_value());
    CHECK_FALSE(parseLayout(separateText("")).has_value());
    CHECK_FALSE(parseLayout(separateText("player A 0 0 380 514", "")).has_value());
    CHECK_FALSE(parseLayout(separateText("player A 0 0 380 514", "library 0 600 720 640", "")).has_value());
    CHECK_FALSE(
        parseLayout(separateText("player A 0 0 380 514", "library 0 600 720 640", "mixer open 900 30 440 250", ""))
            .has_value());
    CHECK_FALSE(parseLayout(separateText("player A 0 0 380 514", "library 0 600 720 640", "mixer open 900 30 440 250",
                                         "playlist closed 1300 600 560 640", ""))
                    .has_value());
    // Lines that only look like known ones
    CHECK_FALSE(parseLayout(separateText("player A 0 0 380 514") + "shelf 0 0 700 600\n").has_value());
    CHECK_FALSE(parseLayout(separateText("player A 0 0 380 514") + "shelf open 0 0 500 500\n").has_value());
    // Player options only on player lines
    CHECK_FALSE(parseLayout(separateText("player A 0 0 380 514", "library 0 600 720 640",
                                         "mixer open 900 30 440 250 remaining"))
                    .has_value());
}

TEST_CASE("Layout: the one window needs its crossfader line; the window line once, with two numbers")
{
    Layout layout          = fiveWithMixer();
    layout.mode            = WindowMode::Mdi;
    layout.mdi             = defaultMdiLayout();
    const std::string good = formatLayout(layout);
    REQUIRE(parseLayout(good).has_value());

    std::string without_mixer = good;
    const auto  mixer_line    = without_mixer.find("mdi mixer");
    without_mixer.erase(mixer_line, without_mixer.find('\n', mixer_line) + 1 - mixer_line);
    CHECK_FALSE(parseLayout(without_mixer).has_value());

    CHECK(parseLayout(good + "mdi window maximized\n").has_value());
    CHECK_FALSE(parseLayout(good + "mdi window maximized\nmdi window maximized\n").has_value());
    CHECK_FALSE(parseLayout(good + "mdi window 800 600 7\n").has_value());

    // The window size is written only when both are known
    layout.mdi->maximized     = false;
    layout.mdi->window_width  = 0;
    layout.mdi->window_height = 600;
    CHECK(formatLayout(layout).find("mdi window") == std::string::npos);
    layout.mdi->window_width  = 800;
    layout.mdi->window_height = 0;
    CHECK(formatLayout(layout).find("mdi window") == std::string::npos);
}

TEST_CASE("Layout: the playlist's place in the one window comes back; a player without a place has none")
{
    Layout layout          = fiveWithMixer();
    layout.mode            = WindowMode::Mdi;
    layout.mdi             = defaultMdiLayout();
    layout.mdi->fullscreen = true;
    layout.mdi->places     = MdiPlaces{.area     = Placement{0, 0, 1920, 1080},
                                       .players  = {Placement{0, 0, 380, 540}}, // only the first player
                                       .mixer    = std::nullopt,
                                       .playlist = Placement{1200, 560, 600, 400},
                                       .beats    = std::nullopt,
                                       .library  = Placement{0, 560, 1000, 400}};
    const std::string text = formatLayout(layout);
    CHECK(text.find("mdi player A 0 0 380 540\n") != std::string::npos);
    CHECK(text.find("mdi player B\n") != std::string::npos);
    CHECK(text.find("mdi playlist closed 1200 560 600 400\n") != std::string::npos);
    const auto again = parseLayout(text);
    REQUIRE(again.has_value());
    REQUIRE(again->mdi.has_value());
    REQUIRE(again->mdi->places.has_value());
    CHECK(again->mdi->places->playlist == Placement{1200, 560, 600, 400});
}

TEST_CASE("Layout: the default places, to the point: 12 apart, centered, the beats between the players with the "
          "crossfader below flush with the players, the library below")
{
    const DefaultSizes sizes{380, 514, 440, 250, 720, 640};
    const Layout       wide = defaultLayout({0, 0, 1920, 1080}, sizes);
    // 380 + 12 + 440 + 12 + 380 = 1224, centered in 1920
    CHECK(wide.players[0].place == Placement{348, 0, 380, 514});
    CHECK(wide.beats.open);
    CHECK(wide.beats_vertical);
    CHECK(wide.beats.place == Placement{740, 0, 440, 216});   // 514 - 250 - 48 for the frames
    CHECK(wide.mixer.place == Placement{740, 264, 440, 250}); // flush with the bottom of the players
    CHECK(wide.players[1].place == Placement{1192, 0, 380, 514});
    // Below: 514 + 48, as high as the rest of the screen allows less 12, at most the size asked for
    CHECK(wide.library == Placement{348, 562, 1224, 506});
    CHECK(wide.playlist.place == Placement{1360, 440, 560, 640}); // moved in: right of the screen, up to fit

    // Narrower than the group: from the left edge, not before it
    const Layout narrow = defaultLayout({0, 0, 1000, 1080}, sizes);
    CHECK(narrow.players[0].place.x == 0);
}

TEST_CASE("Layout: the beat waveforms keep whether they are open, where and how they stand, in both modes")
{
    Layout layout         = fiveWithMixer();
    layout.beats          = {.open = true, .place = {100, 700, 900, 220}};
    layout.beats_vertical = true;
    std::string text      = formatLayout(layout);
    CHECK(text.find("\nbeats open vertical 100 700 900 220\n") != std::string::npos);
    auto again = parseLayout(text);
    REQUIRE(again.has_value());
    CHECK(*again == layout);

    layout.beats_vertical = false;
    layout.beats.open     = false;
    text                  = formatLayout(layout);
    CHECK(text.find("\nbeats closed 100 700 900 220\n") != std::string::npos);
    CHECK(*parseLayout(text) == layout);

    // The one window: open or closed, with a place once the parts were placed by hand
    layout.mode            = WindowMode::Mdi;
    layout.mdi             = defaultMdiLayout();
    layout.mdi->maximized  = true; // what a layout without a window size reads as
    layout.mdi->beats_open = true;
    text                   = formatLayout(layout);
    CHECK(text.find("\nmdi beats open\n") != std::string::npos);
    CHECK(*parseLayout(text) == layout);
    layout.mdi->places = MdiPlaces{.area     = Placement{0, 0, 1920, 1080},
                                   .players  = {std::nullopt, std::nullopt},
                                   .mixer    = std::nullopt,
                                   .playlist = std::nullopt,
                                   .beats    = Placement{0, 600, 1920, 230},
                                   .library  = Placement{0, 840, 1000, 200}};
    text               = formatLayout(layout);
    CHECK(text.find("\nmdi beats open 0 600 1920 230\n") != std::string::npos);
    CHECK(*parseLayout(text) == layout);
}

TEST_CASE("Layout: a layout without its beats lines is no layout")
{
    Layout layout          = fiveWithMixer();
    layout.mdi             = defaultMdiLayout();
    const std::string text = formatLayout(layout);
    for (const std::string_view line :
         {std::string_view{"beats closed 0 0 760 200\n"}, std::string_view{"mdi beats open\n"}})
    {
        std::string without = text;
        const auto  at      = without.find(line);
        REQUIRE(at != std::string::npos);
        without.erase(at, line.size());
        CHECK_FALSE(parseLayout(without).has_value());
    }
}

TEST_CASE("Layout: a damaged beats line, or one twice, is no layout")
{
    Layout layout          = fiveWithMixer();
    layout.mdi             = defaultMdiLayout();
    const std::string good = formatLayout(layout);
    REQUIRE(parseLayout(good).has_value());
    CHECK_FALSE(parseLayout(good + "beats open 0 0 760 200\n").has_value()); // twice
    CHECK_FALSE(parseLayout(good + "mdi beats open\n").has_value());         // twice
    std::string text = good;
    text.replace(text.find("beats closed"), 12, "beats shut");
    CHECK_FALSE(parseLayout(text).has_value()); // neither open nor closed
    text = good;
    text.replace(text.find("beats closed"), 12, "beats closed sideways");
    CHECK_FALSE(parseLayout(text).has_value()); // no such word
    text = good;
    text.replace(text.find("760 200"), 7, "760 20");
    CHECK_FALSE(parseLayout(text).has_value()); // too small
    text = good;
    text.replace(text.find("760 200\n"), 8, "760\n");
    CHECK_FALSE(parseLayout(text).has_value()); // a number missing
}

TEST_CASE("Layout: the one window by default shows the beat waveforms, the crossfader and no playlist")
{
    const MdiLayout parts = defaultMdiLayout();
    CHECK(parts.beats_open);
    CHECK(parts.mixer_open);
    CHECK_FALSE(parts.playlist_open);
}
