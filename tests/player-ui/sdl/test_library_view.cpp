// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library_view.hpp"
#include "track_columns.hpp"

#include "recording_canvas.hpp"

#include <doctest/doctest.h>

#include <utility>

using namespace djw::player_ui::sdl;
namespace api = djw::core::api;
using namespace std::chrono_literals;

namespace
{
    /// Library with fixed tracks: search returns those whose title contains the text
    class FakeLibrary final : public api::Library
    {
        public:
            std::expected<void, std::string>   addFolder(const std::filesystem::path &) override { return {}; }
            void                               rescan() override {}
            std::vector<std::filesystem::path> folders() const override { return {}; }
            api::LibraryStatus                 status() const override { return library_status; }

            void updateTags(std::int64_t track_id, api::LibraryTagChange change) override
            {
                edits.emplace_back(track_id, std::move(change));
            }

            std::expected<std::vector<api::LibraryTrack>, std::string>
            search(const api::LibraryQuery &query) const override
            {
                ++searches;
                last_query              = query;
                const std::string &text = query.text;
                if (fail)
                {
                    return std::unexpected{std::string{"database broken"}};
                }
                std::vector<api::LibraryTrack> found;
                for (const auto &track : tracks)
                {
                    if (track.title.find(text) != std::string::npos)
                    {
                        found.push_back(track);
                    }
                }
                return found;
            }

            std::vector<api::LibraryTrack> tracks{
                {.id = 1, .path = "/m/a.mp3", .artist = "A", .title = "alpha"},
                {.id = 2, .path = "/m/b.mp3", .artist = "B", .title = "beta"},
            };
            api::LibraryStatus                                          library_status;
            std::vector<std::pair<std::int64_t, api::LibraryTagChange>> edits;
            bool                                                        fail     = false;
            mutable int                                                 searches = 0;
            mutable api::LibraryQuery                                   last_query;
    };

    struct Fixture
    {
            Fixture()
            {
                view.layout({0, 0, 600, 400});
                view.addTo(router);
                view.update();
            }

            void tap(Point position)
            {
                router.handle({{1, 1}, PointerPhase::Down, position});
                router.handle({{1, 1}, PointerPhase::Up, position});
            }

            [[nodiscard]] Point firstRow() const
            {
                const Rect &area = view.trackList().bounds();
                return {area.x + 20, area.y + (TrackList::row_height / 2)};
            }

            [[nodiscard]] Point searchField() const
            {
                const Rect &area = view.searchField().bounds();
                return {area.x + 20, area.y + 20};
            }

            FakeLibrary                           library;
            std::vector<api::LibraryTrack>        loaded;
            std::chrono::steady_clock::time_point now{};
            std::filesystem::path                 in_use{"/m/b.mp3"};
            LibraryView   view{library, [this](const api::LibraryTrack &track) { loaded.push_back(track); },
                               [this](const std::filesystem::path &file) { return file == in_use; },
                               [this] { return now; }};
            PointerRouter router;
    };
} // namespace

TEST_CASE("LibraryView: lists all tracks at first, up to the result limit")
{
    Fixture fixture;
    CHECK(fixture.view.trackList().tracks().size() == 2);
    CHECK(fixture.library.last_query.limit == LibraryView::result_limit);
}

TEST_CASE("LibraryView: typing in the search field searches as you type")
{
    Fixture fixture;
    fixture.tap(fixture.searchField());
    CHECK(fixture.view.wantsTextInput());

    fixture.view.keyboard(std::string{"bet"});
    fixture.view.update();

    REQUIRE(fixture.view.trackList().tracks().size() == 1);
    CHECK(fixture.view.trackList().tracks().front().title == "beta");
}

TEST_CASE("LibraryView: typing is ignored while the search field is not focused, no search without a change")
{
    Fixture fixture;
    CHECK_FALSE(fixture.view.wantsTextInput());
    fixture.view.keyboard(std::string{"bet"});
    const int searches = fixture.library.searches;

    fixture.view.update();

    CHECK(fixture.library.searches == searches);
    CHECK(fixture.view.trackList().tracks().size() == 2);
}

TEST_CASE("LibraryView: a double tap on a track loads it")
{
    Fixture fixture;
    fixture.tap(fixture.firstRow());
    fixture.now += 100ms;
    fixture.tap(fixture.firstRow());
    fixture.view.update();

    REQUIRE(fixture.loaded.size() == 1);
    CHECK(fixture.loaded.front().path == std::filesystem::path{"/m/a.mp3"});
}

TEST_CASE("LibraryView: while scanning the list is refreshed once per interval, and when the revision changes")
{
    Fixture fixture;
    fixture.library.library_status.scanning = true;
    const int before                        = fixture.library.searches;

    fixture.view.update();
    CHECK(fixture.library.searches == before);

    fixture.now += LibraryView::refresh_interval;
    fixture.view.update();
    CHECK(fixture.library.searches == before + 1);

    fixture.library.library_status.scanning = false;
    ++fixture.library.library_status.revision; // the scan ended
    fixture.view.update();
    CHECK(fixture.library.searches == before + 2);

    fixture.now += LibraryView::refresh_interval;
    fixture.view.update();
    CHECK(fixture.library.searches == before + 2);
}

TEST_CASE("LibraryView: the status line shows the track count, the scan progress and errors")
{
    Fixture fixture;
    fixture.library.library_status = {.scanning = true, .files_found = 12, .tracks_total = 340};
    fixture.view.update();
    CHECK(fixture.view.statusText() == "340 tracks - scanning, 12 files");

    fixture.library.library_status = {.tracks_total = 340, .scan_error = "cannot read folder /x"};
    fixture.view.update();
    CHECK(fixture.view.statusText() == "340 tracks - cannot read folder /x");

    djw::tests::RecordingCanvas canvas;
    fixture.view.draw(canvas);
    CHECK(canvas.drew("340 tracks - cannot read folder /x"));
}

TEST_CASE("LibraryView: a failing search shows its error and keeps the list")
{
    Fixture fixture;
    fixture.tap(fixture.searchField());
    fixture.library.fail = true;
    fixture.view.keyboard(std::string{"x"});
    fixture.view.update();

    CHECK(fixture.view.trackList().tracks().size() == 2);
    CHECK(fixture.view.statusText().find("database broken") != std::string::npos);
}

TEST_CASE("LibraryView: the mouse wheel scrolls the list only over the list")
{
    Fixture fixture;
    fixture.library.tracks.resize(50, fixture.library.tracks.front());
    fixture.tap(fixture.searchField());
    fixture.view.keyboard(std::string{"a"});
    fixture.view.update();

    fixture.view.wheel(fixture.searchField(), -1.0F);
    CHECK(fixture.view.trackList().scrollOffset() == 0.0F);

    fixture.view.wheel(fixture.firstRow(), -1.0F);
    CHECK(fixture.view.trackList().scrollOffset() == doctest::Approx(TrackList::row_height));
}

namespace
{
    Point centerOf(const Rect &area)
    {
        return {area.x + (area.width / 2), area.y + (area.height / 2)};
    }
} // namespace

TEST_CASE("LibraryView: the filter fields sit above their columns and go into the query")
{
    Fixture fixture;
    for (std::size_t index = 0; index < LibraryView::filter_columns.size(); ++index)
    {
        const Rect &field  = fixture.view.filterField(index).bounds();
        const Rect  column = columnArea(fixture.view.trackList().bounds(), LibraryView::filter_columns.at(index));
        CHECK(field.x == doctest::Approx(column.x));
        CHECK(field.y + field.height <= fixture.view.columnHeader().bounds().y);
    }

    fixture.tap(centerOf(fixture.view.filterField(0).bounds()));
    fixture.view.keyboard(std::string{"daft"});
    fixture.tap(centerOf(fixture.view.filterField(3).bounds()));
    fixture.view.keyboard(std::string{"199?"});
    fixture.view.update();

    CHECK(fixture.library.last_query.artist == "daft");
    CHECK(fixture.library.last_query.year == "199?");
    CHECK(fixture.library.last_query.title.empty());
    CHECK(fixture.view.query() == fixture.library.last_query);
}

TEST_CASE("LibraryView: typed text goes only to the field tapped last")
{
    Fixture fixture;
    fixture.tap(fixture.searchField());
    fixture.tap(centerOf(fixture.view.filterField(1).bounds()));

    fixture.view.keyboard(std::string{"x"});

    CHECK(fixture.view.searchField().text().empty());
    CHECK(fixture.view.filterField(1).text() == "x");
    CHECK_FALSE(fixture.view.searchField().focused());
    CHECK(fixture.view.wantsTextInput());
}

TEST_CASE("LibraryView: tapping a column header sorts by it, tapping it again reverses the order")
{
    Fixture    fixture;
    const auto tap_column = [&](api::LibraryColumn column)
    {
        fixture.tap(centerOf(columnArea(fixture.view.columnHeader().bounds(), column)));
        fixture.view.update();
    };
    CHECK(fixture.library.last_query.sort_by == api::LibraryColumn::Artist);

    tap_column(api::LibraryColumn::Year);
    CHECK(fixture.library.last_query.sort_by == api::LibraryColumn::Year);
    CHECK_FALSE(fixture.library.last_query.descending);

    tap_column(api::LibraryColumn::Year);
    CHECK(fixture.library.last_query.descending);

    tap_column(api::LibraryColumn::Title);
    CHECK(fixture.library.last_query.sort_by == api::LibraryColumn::Title);
    CHECK_FALSE(fixture.library.last_query.descending);
}

namespace
{
    /// Holds a cell of the list for the long press time, then releases it
    void longPress(Fixture &fixture, std::size_t row, api::LibraryColumn column)
    {
        const Rect  list = fixture.view.trackList().bounds();
        const Rect  cell = columnArea(list, column);
        const Point at{cell.x + 10, list.y + (TrackList::row_height * static_cast<float>(row)) + 10};
        fixture.router.handle({{1, 1}, PointerPhase::Down, at});
        fixture.now += TrackList::long_press_time;
        fixture.view.update();
        fixture.router.handle({{1, 1}, PointerPhase::Up, at});
    }

    void press(Fixture &fixture, EditKey key)
    {
        fixture.view.keyboard(key);
        fixture.view.update();
    }
} // namespace

TEST_CASE("LibraryView: a long press on a cell edits it in place, Enter saves into the library and the row")
{
    Fixture fixture;
    longPress(fixture, 0, api::LibraryColumn::Title);

    REQUIRE(fixture.view.editing());
    CHECK(fixture.view.cellEditor().text() == "alpha");
    CHECK(fixture.view.cellEditor().focused());
    CHECK(fixture.view.wantsTextInput());
    CHECK(fixture.view.cellEditor().bounds().x ==
          doctest::Approx(columnArea(fixture.view.trackList().bounds(), api::LibraryColumn::Title).x));

    fixture.view.keyboard(EditKey::Backspace);
    fixture.view.keyboard(std::string{"ö"});
    press(fixture, EditKey::Enter);

    CHECK_FALSE(fixture.view.editing());
    REQUIRE(fixture.library.edits.size() == 1);
    CHECK(fixture.library.edits.front().first == 1);
    CHECK(fixture.library.edits.front().second == api::LibraryTagChange{.title = "alphö"});
    CHECK(fixture.view.trackList().tracks().front().title == "alphö");
    CHECK(fixture.loaded.empty()); // the release after the long press is no tap
}

TEST_CASE("LibraryView: Escape cancels an edit, an unchanged value is not written")
{
    Fixture fixture;
    longPress(fixture, 0, api::LibraryColumn::Artist);
    fixture.view.keyboard(std::string{"x"});
    press(fixture, EditKey::Escape);
    CHECK_FALSE(fixture.view.editing());

    longPress(fixture, 0, api::LibraryColumn::Artist);
    press(fixture, EditKey::Enter);

    CHECK(fixture.library.edits.empty());
    CHECK(fixture.view.trackList().tracks().front().artist == "A");
}

TEST_CASE("LibraryView: touching the list or another field saves the edit")
{
    Fixture fixture;
    longPress(fixture, 0, api::LibraryColumn::Genre);
    fixture.view.keyboard(std::string{"House"});
    fixture.tap(fixture.searchField());
    fixture.view.update();

    CHECK_FALSE(fixture.view.editing());
    CHECK(fixture.view.searchField().focused());
    REQUIRE(fixture.library.edits.size() == 1);
    CHECK(fixture.library.edits.back().second == api::LibraryTagChange{.genre = "House"});

    longPress(fixture, 0, api::LibraryColumn::Genre);
    fixture.view.keyboard(std::string{" Deep"});
    fixture.tap({fixture.firstRow().x, fixture.firstRow().y + TrackList::row_height});
    fixture.view.update();

    CHECK_FALSE(fixture.view.editing());
    REQUIRE(fixture.library.edits.size() == 2);
    CHECK(fixture.library.edits.back().second == api::LibraryTagChange{.genre = "House Deep"});
}

TEST_CASE("LibraryView: the year must be a number, empty removes it")
{
    Fixture fixture;
    longPress(fixture, 0, api::LibraryColumn::Year);
    fixture.view.keyboard(std::string{"19x9"});
    press(fixture, EditKey::Enter);
    CHECK(fixture.library.edits.empty());
    CHECK(fixture.view.statusText().find("year must be a number") != std::string::npos);

    longPress(fixture, 0, api::LibraryColumn::Year);
    fixture.view.keyboard(std::string{"1999"});
    press(fixture, EditKey::Enter);
    REQUIRE(fixture.library.edits.size() == 1);
    CHECK(fixture.library.edits.back().second == api::LibraryTagChange{.year = 1999});
    CHECK(fixture.view.trackList().tracks().front().year == 1999);

    longPress(fixture, 0, api::LibraryColumn::Year);
    CHECK(fixture.view.cellEditor().text() == "1999");
    fixture.view.keyboard(EditKey::Backspace);
    fixture.view.keyboard(EditKey::Backspace);
    fixture.view.keyboard(EditKey::Backspace);
    fixture.view.keyboard(EditKey::Backspace);
    press(fixture, EditKey::Enter);
    CHECK(fixture.library.edits.back().second == api::LibraryTagChange{.year = 0});
}

TEST_CASE("LibraryView: the duration cannot be edited, nor a track loaded in a deck")
{
    Fixture fixture;
    longPress(fixture, 0, api::LibraryColumn::Duration);
    CHECK_FALSE(fixture.view.editing());

    longPress(fixture, 1, api::LibraryColumn::Title); // /m/b.mp3 is in use
    CHECK_FALSE(fixture.view.editing());
    CHECK(fixture.view.statusText().find("loaded in a deck") != std::string::npos);
}

TEST_CASE("LibraryView: pending tag edits and their errors show in the status line")
{
    Fixture fixture;
    fixture.library.library_status = {.tracks_total = 2, .pending_edits = 1};
    fixture.view.update();
    CHECK(fixture.view.statusText() == "2 tracks - saving tags");

    fixture.library.library_status = {.tracks_total = 2, .edit_error = "cannot write tags of /m/a.mp3"};
    fixture.view.update();
    CHECK(fixture.view.statusText() == "2 tracks - cannot write tags of /m/a.mp3");
}
