// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library_view.hpp"
#include "track_columns.hpp"

#include "recording_canvas.hpp"

#include <doctest/doctest.h>

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
            api::LibraryScanStatus             scanStatus() const override { return status; }

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
            api::LibraryScanStatus    status;
            bool                      fail     = false;
            mutable int               searches = 0;
            mutable api::LibraryQuery last_query;
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
            LibraryView   view{library, [this](const api::LibraryTrack &track) { loaded.push_back(track); },
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

TEST_CASE("LibraryView: while scanning the list is refreshed once per interval and when the scan ends")
{
    Fixture fixture;
    fixture.library.status.running = true;
    const int before               = fixture.library.searches;

    fixture.view.update();
    CHECK(fixture.library.searches == before);

    fixture.now += LibraryView::refresh_interval;
    fixture.view.update();
    CHECK(fixture.library.searches == before + 1);

    fixture.library.status.running = false;
    fixture.view.update();
    CHECK(fixture.library.searches == before + 2);

    fixture.now += LibraryView::refresh_interval;
    fixture.view.update();
    CHECK(fixture.library.searches == before + 2);
}

TEST_CASE("LibraryView: the status line shows the track count, the scan progress and errors")
{
    Fixture fixture;
    fixture.library.status = {.running = true, .files_found = 12, .tracks_total = 340};
    fixture.view.update();
    CHECK(fixture.view.statusText() == "340 tracks - scanning, 12 files");

    fixture.library.status = {.tracks_total = 340, .error = "cannot read folder /x"};
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
