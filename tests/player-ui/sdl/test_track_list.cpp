// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "track_columns.hpp"
#include "track_list.hpp"

#include "recording_canvas.hpp"

#include <doctest/doctest.h>

using namespace djw::player_ui::sdl;
namespace api = djw::core::api;
using namespace std::chrono_literals;

namespace
{
    constexpr float row = TrackList::row_height;

    std::vector<api::LibraryTrack> tracks(int count)
    {
        std::vector<api::LibraryTrack> result;
        for (int i = 0; i < count; ++i)
        {
            result.push_back({.id               = i,
                              .artist           = "Artist " + std::to_string(i),
                              .title            = "Title " + std::to_string(i),
                              .year             = i == 0 ? 0 : 1990 + i,
                              .duration_seconds = 245.0});
        }
        return result;
    }

    /// A list 5 rows high with a clock the test moves
    struct Fixture
    {
            Fixture()
            {
                list.setBounds({0, 100, 600, 5 * row});
                list.setTracks(tracks(20));
            }

            void tap(float y)
            {
                list.pointerDown({50, y});
                list.pointerUp({50, y});
            }

            /// y of the middle of a visible row
            [[nodiscard]] float rowY(int visible_row) const { return 100 + (row * visible_row) + (row / 2); }

            std::chrono::steady_clock::time_point now{};
            TrackList                             list{[this] { return now; }};
    };
} // namespace

TEST_CASE("TrackList: a tap selects the row under it")
{
    Fixture fixture;
    fixture.tap(fixture.rowY(2));
    CHECK(fixture.list.selected() == 2U);
    CHECK_FALSE(fixture.list.takeActivated());
}

TEST_CASE("TrackList: a second tap on the same row within the double tap time activates it, once")
{
    Fixture fixture;
    fixture.tap(fixture.rowY(1));
    fixture.now += 300ms;
    fixture.tap(fixture.rowY(1));

    CHECK(fixture.list.takeActivated() == 1U);
    CHECK_FALSE(fixture.list.takeActivated());
}

TEST_CASE("TrackList: taps too far apart in time or on different rows do not activate")
{
    Fixture fixture;
    fixture.tap(fixture.rowY(1));
    fixture.now += 500ms;
    fixture.tap(fixture.rowY(1));
    CHECK_FALSE(fixture.list.takeActivated());

    fixture.now += 100ms;
    fixture.tap(fixture.rowY(2));
    CHECK_FALSE(fixture.list.takeActivated());
    CHECK(fixture.list.selected() == 2U);
}

TEST_CASE("TrackList: dragging scrolls and does not select, the scroll stops at both ends")
{
    Fixture fixture;
    fixture.list.pointerDown({50, 300});
    fixture.list.pointerMove({50, 300 - (2 * row)});
    fixture.list.pointerUp({50, 300 - (2 * row)});

    CHECK(fixture.list.scrollOffset() == doctest::Approx(2 * row));
    CHECK_FALSE(fixture.list.selected());

    fixture.list.pointerDown({50, 100});
    fixture.list.pointerMove({50, 2'000});
    fixture.list.pointerUp({50, 2'000});
    CHECK(fixture.list.scrollOffset() == 0.0F);

    fixture.list.scrollBy(-100'000);
    CHECK(fixture.list.scrollOffset() == doctest::Approx(15 * row)); // 20 rows, 5 visible
}

TEST_CASE("TrackList: a small wobble while tapping is still a tap")
{
    Fixture fixture;
    fixture.list.pointerDown({50, fixture.rowY(0)});
    fixture.list.pointerMove({52, fixture.rowY(0) + TrackList::tap_slop - 1});
    fixture.list.pointerUp({52, fixture.rowY(0) + TrackList::tap_slop - 1});

    CHECK(fixture.list.selected() == 0U);
    CHECK(fixture.list.scrollOffset() == 0.0F);
}

TEST_CASE("TrackList: a tap after scrolling selects the row that is shown there")
{
    Fixture fixture;
    fixture.list.scrollBy(-3 * row);
    fixture.tap(fixture.rowY(0));
    CHECK(fixture.list.selected() == 3U);
}

TEST_CASE("TrackList: a tap below the last row selects nothing")
{
    Fixture fixture;
    fixture.list.setTracks(tracks(2));
    fixture.tap(fixture.rowY(4));
    CHECK_FALSE(fixture.list.selected());
}

TEST_CASE("TrackList: new tracks reset scroll and selection, unless the position is kept")
{
    Fixture fixture;
    fixture.list.scrollBy(-3 * row);
    fixture.tap(fixture.rowY(0));

    fixture.list.setTracks(tracks(20), true);
    CHECK(fixture.list.scrollOffset() == doctest::Approx(3 * row));
    CHECK(fixture.list.selected() == 3U);

    fixture.list.setTracks(tracks(20));
    CHECK(fixture.list.scrollOffset() == 0.0F);
    CHECK_FALSE(fixture.list.selected());
}

TEST_CASE("TrackList: draws the visible rows only, clipped to its bounds, with the duration")
{
    Fixture                     fixture;
    djw::tests::RecordingCanvas canvas;
    fixture.list.draw(canvas);

    CHECK(canvas.drew("Artist 0"));
    CHECK(canvas.drew("Title 4"));
    CHECK_FALSE(canvas.drew("Title 5"));
    CHECK(canvas.drew("4:05"));
    CHECK(canvas.drew("1991"));
    CHECK_FALSE(canvas.drew("0")); // no year: empty
    REQUIRE(canvas.clip_changes.size() == 2);
    CHECK(canvas.clip_changes.front().has_value());
    CHECK_FALSE(canvas.clip.has_value()); // reset after drawing
}

TEST_CASE("TrackList: holding a cell still presses it long once, the release is no tap")
{
    Fixture     fixture;
    const Rect  title = columnArea(fixture.list.bounds(), api::LibraryColumn::Title);
    const Point at{title.x + 5, fixture.rowY(2)};
    fixture.list.pointerDown(at);
    fixture.now += TrackList::long_press_time - 1ms;
    CHECK_FALSE(fixture.list.takeLongPress());

    fixture.now += 1ms;
    const auto cell = fixture.list.takeLongPress();
    REQUIRE(cell.has_value());
    CHECK(*cell == TrackList::Cell{2, api::LibraryColumn::Title});
    CHECK_FALSE(fixture.list.takeLongPress());

    fixture.list.pointerUp(at);
    fixture.now += 100ms;
    fixture.tap(fixture.rowY(2)); // would be a double tap if the release had counted
    CHECK_FALSE(fixture.list.takeActivated());
    CHECK(fixture.list.takeTouched());
}

TEST_CASE("TrackList: a drag is never a long press")
{
    Fixture fixture;
    fixture.list.pointerDown({50, 300});
    fixture.list.pointerMove({50, 300 - (2 * TrackList::tap_slop)});
    fixture.now += 2 * TrackList::long_press_time;
    CHECK_FALSE(fixture.list.takeLongPress());
}

TEST_CASE("TrackList: cell areas follow the scroll position and are empty out of view")
{
    Fixture    fixture;
    const auto area = fixture.list.cellArea({1, api::LibraryColumn::Artist});
    REQUIRE(area.has_value());
    CHECK(area->y == doctest::Approx(100 + TrackList::row_height));

    fixture.list.scrollBy(-2 * TrackList::row_height);
    CHECK_FALSE(fixture.list.cellArea({1, api::LibraryColumn::Artist}).has_value());
    CHECK(fixture.list.cellArea({3, api::LibraryColumn::Artist})->y == doctest::Approx(100 + TrackList::row_height));
    CHECK_FALSE(fixture.list.cellArea({99, api::LibraryColumn::Artist}).has_value());
}

TEST_CASE("TrackList: a row can be replaced in place")
{
    Fixture fixture;
    auto    track = fixture.list.tracks()[2];
    track.title   = "Edited";
    fixture.list.replaceTrack(2, track);
    fixture.list.replaceTrack(99, track); // ignored
    CHECK(fixture.list.tracks()[2].title == "Edited");
    CHECK(fixture.list.tracks().size() == 20);
}
