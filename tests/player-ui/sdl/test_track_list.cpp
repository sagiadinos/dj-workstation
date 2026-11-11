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

TEST_CASE("TrackList: the BPM column shows the tempo with one decimal, nothing for a track without one")
{
    Fixture fixture;
    auto    rows = tracks(2);
    rows[0].bpm  = 127.96;
    fixture.list.setTracks(rows);

    djw::tests::RecordingCanvas canvas;
    fixture.list.draw(canvas);
    CHECK(canvas.drew("128.0"));
    CHECK(std::ranges::count_if(canvas.texts, [](const auto &drawn)
                                { return drawn.text.find('.') != std::string::npos && drawn.text != "4:05"; }) == 1);
}

TEST_CASE("TrackList: the cursor keys move the selection a row, the page keys a page, and it stays in view")
{
    Fixture fixture; // 20 rows, 5 in view
    CHECK(fixture.list.pageRows() == 5);

    // Without a selection Down starts at the first row, Up at the last
    CHECK(fixture.list.navigate(EditKey::Down));
    CHECK(fixture.list.selected() == 0U);
    fixture.list.navigate(EditKey::Up); // at the top: stays
    CHECK(fixture.list.selected() == 0U);

    fixture.list.navigate(EditKey::PageDown);
    CHECK(fixture.list.selected() == 5U);
    // Row 5 is the sixth: scrolled so it is the last one in view
    CHECK(fixture.list.scrollOffset() == doctest::Approx(row));

    for (int i = 0; i < 5; ++i)
    {
        fixture.list.navigate(EditKey::PageDown);
    }
    CHECK(fixture.list.selected() == 19U); // the end, no further
    CHECK(fixture.list.scrollOffset() == doctest::Approx(15 * row));

    fixture.list.navigate(EditKey::PageUp);
    CHECK(fixture.list.selected() == 14U);
    CHECK(fixture.list.scrollOffset() == doctest::Approx(14 * row)); // at the top edge now

    CHECK_FALSE(fixture.list.navigate(EditKey::Enter)); // not a moving key

    TrackList empty;
    empty.moveSelection(1);
    CHECK_FALSE(empty.selected().has_value());
    TrackList fresh;
    fresh.setBounds({0, 0, 600, 5 * row});
    fresh.setTracks(tracks(3));
    fresh.navigate(EditKey::Up);
    CHECK(fresh.selected() == 2U);
}

namespace
{
    /// A quick drag upwards (the list scrolls towards its end): 60 points in 30 ms, then let go
    void flick(Fixture &fixture, float distance = 60.0F)
    {
        fixture.list.pointerDown({50, 300});
        for (int step = 1; step <= 3; ++step)
        {
            fixture.now += 10ms;
            fixture.list.pointerMove({50, 300 - (distance * static_cast<float>(step) / 3)});
        }
        fixture.list.pointerUp({50, 300 - distance});
    }
} // namespace

TEST_CASE("TrackList: let go while moving, the list runs on, slows down and stops")
{
    Fixture fixture; // 20 rows, 5 in view
    flick(fixture);
    const float let_go = fixture.list.scrollOffset();
    CHECK(let_go == doctest::Approx(60.0F));
    REQUIRE(fixture.list.flinging());
    CHECK_FALSE(fixture.list.selected().has_value()); // a drag selects nothing

    fixture.now += 100ms;
    fixture.list.advance();
    const float after_100 = fixture.list.scrollOffset();
    CHECK(after_100 > let_go);
    fixture.now += 100ms;
    fixture.list.advance();
    const float after_200 = fixture.list.scrollOffset();
    CHECK(after_200 > after_100);
    CHECK(after_200 - after_100 < after_100 - let_go); // slower and slower

    fixture.now += 5s;
    fixture.list.advance();
    CHECK_FALSE(fixture.list.flinging());
    // At 2000 points per second and a time constant of 0.325 s it runs some 650 points at most
    CHECK(fixture.list.scrollOffset() <= let_go + 650.0F + 1.0F);
}

TEST_CASE("TrackList: a fling stops at the end of the list, at a touch, at the wheel and at the keys")
{
    Fixture fixture;
    flick(fixture, 90.0F);
    fixture.now += 3s;
    fixture.list.advance();
    CHECK(fixture.list.scrollOffset() == doctest::Approx(15 * row)); // the end: 20 rows, 5 in view
    CHECK_FALSE(fixture.list.flinging());

    for (const int stop : {0, 1, 2})
    {
        CAPTURE(stop);
        fixture.list.scrollBy(1'000.0F); // back to the top
        flick(fixture);
        REQUIRE(fixture.list.flinging());
        if (stop == 0)
        {
            fixture.list.pointerDown({50, 200});
            fixture.list.pointerUp({50, 200});
        }
        else if (stop == 1)
        {
            fixture.list.scrollBy(-10.0F);
        }
        else
        {
            fixture.list.navigate(djw::player_ui::sdl::EditKey::Down);
        }
        CHECK_FALSE(fixture.list.flinging());
    }
}

TEST_CASE("TrackList: a finger that came to rest before it lifts, or a slow drag, gives no fling")
{
    Fixture fixture;
    fixture.list.pointerDown({50, 300});
    fixture.now += 10ms;
    fixture.list.pointerMove({50, 250});
    fixture.now += 300ms; // held still
    fixture.list.pointerUp({50, 250});
    CHECK_FALSE(fixture.list.flinging());

    fixture.list.pointerDown({50, 300});
    for (int step = 1; step <= 10; ++step)
    {
        fixture.now += 100ms;
        fixture.list.pointerMove({50, 300.0F - static_cast<float>(step) * 3.0F});
    }
    fixture.list.pointerUp({50, 270});
    CHECK_FALSE(fixture.list.flinging()); // 30 points per second: too slow
}
