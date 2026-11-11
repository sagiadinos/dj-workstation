// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library_view.hpp"
#include "fake_dialogs.hpp"
#include "track_columns.hpp"

#include "fake_library.hpp"
#include "recording_canvas.hpp"

#include <doctest/doctest.h>

#include <utility>

using namespace djw::player_ui::sdl;
namespace api = djw::core::api;
using namespace std::chrono_literals;
using djw::tests::RecordingCanvas;

namespace
{
    using djw::tests::FakeLibrary;

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
            djw::tests::FakeDialogs               dialogs;
            Translator                            translator;
            LibraryView   view{library,
                               dialogs,
                               translator,
                               [this](const api::LibraryTrack &track) { loaded.push_back(track); },
                               [this](const std::filesystem::path &file) { return file == in_use; },
                               [this] { return now; }};
            PointerRouter router;
    };
} // namespace

TEST_CASE("LibraryView: lists all tracks at first, up to the result limit")
{
    Fixture fixture;
    CHECK(fixture.view.trackList().tracks().size() == 3);
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
    CHECK(fixture.view.trackList().tracks().size() == 3);
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

    CHECK(fixture.view.trackList().tracks().size() == 3);
    CHECK(fixture.view.statusText().find("database broken") != std::string::npos);
}

TEST_CASE("LibraryView: the mouse wheel scrolls the list only over the list")
{
    Fixture fixture;
    fixture.library.library_tracks.resize(50, fixture.library.library_tracks.front());
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
    CHECK(fixture.view.statusText().find("loaded in a player") != std::string::npos);
}

TEST_CASE("LibraryView: pending tag edits and their errors show in the status line")
{
    Fixture fixture;
    fixture.library.library_status = {.tracks_total = 2, .pending_writes = 1};
    fixture.view.update();
    CHECK(fixture.view.statusText() == "2 tracks - saving tags");

    fixture.library.library_status = {.tracks_total = 2, .write_error = "cannot write tags of /m/a.mp3"};
    fixture.view.update();
    CHECK(fixture.view.statusText() == "2 tracks - cannot write tags of /m/a.mp3");
}

namespace
{
    void tapButton(Fixture &fixture, const TextButton &button)
    {
        const Rect &area = button.bounds();
        REQUIRE(area.width > 0);
        fixture.tap({area.x + (area.width / 2), area.y + (area.height / 2)});
        fixture.view.update();
    }
} // namespace

TEST_CASE("LibraryView: the missing button shows only when tracks are missing and filters on them")
{
    Fixture fixture;
    CHECK(fixture.view.missingButton().bounds().width == 0.0F);
    CHECK(fixture.view.removeButton().bounds().width == 0.0F);

    fixture.library.library_status.missing_total = 3;
    fixture.view.update();
    CHECK(fixture.view.missingButton().label() == "Missing: 3");
    CHECK(fixture.view.removeButton().bounds().width == 0.0F);

    tapButton(fixture, fixture.view.missingButton());
    CHECK(fixture.library.last_query.only_missing);
    CHECK(fixture.view.missingButton().isHighlighted());
    CHECK(fixture.view.removeButton().bounds().width > 0.0F);

    tapButton(fixture, fixture.view.missingButton());
    CHECK_FALSE(fixture.library.last_query.only_missing);
}

TEST_CASE("LibraryView: removing missing tracks needs a second tap in time")
{
    Fixture fixture;
    fixture.library.library_status.missing_total = 2;
    fixture.view.update();
    tapButton(fixture, fixture.view.missingButton());

    tapButton(fixture, fixture.view.removeButton());
    CHECK(fixture.library.removals == 0);
    CHECK(fixture.view.removeButton().label() == "Tap again to remove 2");

    fixture.now += LibraryView::confirm_time + 1ms;
    fixture.view.update();
    CHECK(fixture.view.removeButton().label() == "Remove missing");
    tapButton(fixture, fixture.view.removeButton());
    CHECK(fixture.library.removals == 0);

    fixture.now += 1s;
    tapButton(fixture, fixture.view.removeButton());
    CHECK(fixture.library.removals == 1);
    CHECK_FALSE(fixture.library.last_query.only_missing);
}

TEST_CASE("LibraryView: with no missing tracks left the filter turns off")
{
    Fixture fixture;
    fixture.library.library_status.missing_total = 1;
    fixture.view.update();
    tapButton(fixture, fixture.view.missingButton());
    REQUIRE(fixture.library.last_query.only_missing);

    fixture.library.library_status.missing_total = 0;
    fixture.view.update();
    fixture.view.update();

    CHECK_FALSE(fixture.library.last_query.only_missing);
    CHECK(fixture.view.missingButton().bounds().width == 0.0F);
}

TEST_CASE("LibraryView: the Add to playlist button shows for a selected track and hands it on")
{
    Fixture                        fixture;
    std::vector<api::LibraryTrack> added;
    fixture.view.update();
    CHECK(fixture.view.addButton().bounds().width == 0.0F); // no receiver yet

    fixture.view.setAddToPlaylist([&added](const api::LibraryTrack &track) { added.push_back(track); });
    fixture.view.update();
    CHECK(fixture.view.addButton().bounds().width == 0.0F); // nothing selected

    fixture.tap(fixture.firstRow());
    fixture.view.update();
    tapButton(fixture, fixture.view.addButton());

    REQUIRE(added.size() == 1);
    CHECK(added.front().title == "alpha");
}

namespace
{
    /// Opens menu number menu and taps its entry number item
    void chooseEntry(Fixture &fixture, std::size_t menu, std::size_t item)
    {
        const Rect title = fixture.view.menuBar().titleArea(menu);
        fixture.tap({title.x + 10, title.y + 10});
        const Rect entry = fixture.view.menuBar().itemArea(menu, item);
        fixture.tap({entry.x + 10, entry.y + 10});
        fixture.view.update();
    }
} // namespace

TEST_CASE("LibraryView: the menu bar sits on top with Database, Playlist, Settings, Language and View")
{
    Fixture fixture;
    CHECK(fixture.view.menuBar().bounds().y == 0.0F);
    CHECK(fixture.view.searchField().bounds().y >= MenuBar::bar_height);
    RecordingCanvas canvas;
    fixture.view.draw(canvas);
    CHECK(canvas.drew("Database"));
    CHECK(canvas.drew("Playlist"));
    CHECK(canvas.drew("Settings"));
    CHECK(canvas.drew("Language"));
    CHECK(canvas.drew("View"));
}

TEST_CASE("LibraryView: Add music folder asks for a folder and adds it to the library")
{
    Fixture fixture;
    chooseEntry(fixture, 0, 0);
    CHECK(fixture.dialogs.folder_asked);
    fixture.dialogs.choose("/music");
    REQUIRE(fixture.library.added_folders.size() == 1);
    CHECK(fixture.library.added_folders[0] == "/music");
}

TEST_CASE("LibraryView: a folder the library refuses shows in the status line")
{
    Fixture fixture;
    fixture.library.add_folder_error = "no such folder";
    chooseEntry(fixture, 0, 0);
    fixture.dialogs.choose("/nowhere");
    fixture.view.update();
    CHECK(fixture.view.statusText().find("no such folder") != std::string::npos);
}

TEST_CASE("LibraryView: Rescan scans the library again")
{
    Fixture fixture;
    chooseEntry(fixture, 0, 1);
    CHECK(fixture.library.rescans == 1);
}

TEST_CASE("LibraryView: the Playlist menu is off without a playlist window, otherwise it sends its commands")
{
    Fixture fixture;
    chooseEntry(fixture, 1, 0);
    // Disabled: the menu stays open and nothing was sent
    CHECK(fixture.view.menuBar().isOpen());
    fixture.view.keyboard(EditKey::Escape);

    std::vector<LibraryView::PlaylistCommand> commands;
    fixture.view.setPlaylistCommands([&commands](LibraryView::PlaylistCommand command)
                                     { commands.push_back(command); });
    fixture.view.update();
    chooseEntry(fixture, 1, 0);
    chooseEntry(fixture, 1, 1);
    chooseEntry(fixture, 1, 2);
    CHECK(commands == std::vector{LibraryView::PlaylistCommand::New, LibraryView::PlaylistCommand::Import,
                                  LibraryView::PlaylistCommand::Export});
}

TEST_CASE("LibraryView: Add selected track needs a selected track")
{
    Fixture                   fixture;
    std::vector<std::int64_t> added;
    fixture.view.setAddToPlaylist([&added](const api::LibraryTrack &track) { added.push_back(track.id); });
    fixture.view.setPlaylistCommands([](LibraryView::PlaylistCommand) {});
    fixture.view.update();
    chooseEntry(fixture, 1, 3);
    CHECK(added.empty());
    fixture.view.keyboard(EditKey::Escape); // closes the menu

    fixture.tap(fixture.firstRow());
    fixture.view.update();
    chooseEntry(fixture, 1, 3);
    REQUIRE(added.size() == 1);
    CHECK(added[0] == fixture.view.trackList().tracks()[0].id);
}

TEST_CASE("LibraryView: the Language menu lists the languages and reports the one chosen")
{
    Fixture               fixture;
    std::vector<Language> chosen;
    fixture.view.setOnLanguage([&chosen](Language language) { chosen.push_back(language); });
    chooseEntry(fixture, 3, 1);
    REQUIRE(chosen.size() == 1);
    CHECK(chosen[0] == Language::German);

    const Rect title = fixture.view.menuBar().titleArea(3);
    fixture.tap({title.x + 10, title.y + 10});
    RecordingCanvas canvas;
    fixture.view.draw(canvas);
    for (const Language language : languages)
    {
        CHECK(canvas.drew(languageName(language)));
    }
}

TEST_CASE("LibraryView: View clears the search and the filters, and shows only missing tracks")
{
    Fixture fixture;
    fixture.tap(fixture.searchField());
    fixture.view.keyboard(std::string{"abc"});
    fixture.view.update();
    CHECK(fixture.view.query().text == "abc");
    chooseEntry(fixture, 4, 3);
    fixture.view.update();
    CHECK(fixture.view.query().text.empty());

    // Missing tracks only: disabled without missing tracks
    chooseEntry(fixture, 4, 4);
    CHECK_FALSE(fixture.view.query().only_missing);
    fixture.view.keyboard(EditKey::Escape); // closes the menu
    fixture.library.library_status.missing_total = 2;
    fixture.view.update();
    chooseEntry(fixture, 4, 4);
    fixture.view.update();
    CHECK(fixture.view.query().only_missing);
}

TEST_CASE("LibraryView: all texts follow the language at once")
{
    Fixture fixture;
    fixture.library.library_status.tracks_total  = 1;
    fixture.library.library_status.missing_total = 3;
    fixture.view.update();
    CHECK(fixture.view.statusText() == "1 track");
    CHECK(fixture.view.missingButton().label() == "Missing: 3");

    fixture.translator.setLanguage(Language::Russian);
    fixture.library.library_status.tracks_total = 3;
    fixture.view.update();
    CHECK(fixture.view.statusText() == "3 трека");
    CHECK(fixture.view.missingButton().label() == "Отсутствуют: 3");

    RecordingCanvas canvas;
    fixture.view.draw(canvas);
    // (the recording canvas measures bytes, so only short Cyrillic words fit its menu titles)
    CHECK(canvas.drew("Вид"));
    CHECK(canvas.drew("Поиск"));

    fixture.translator.setLanguage(Language::German);
    fixture.view.update();
    RecordingCanvas german;
    fixture.view.draw(german);
    CHECK(german.drew("Datenbank"));
    CHECK(german.drew("Interpret"));
    CHECK(german.drew("Suche"));
}

TEST_CASE("LibraryView: Escape closes an open menu")
{
    Fixture    fixture;
    const Rect title = fixture.view.menuBar().titleArea(0);
    fixture.tap({title.x + 10, title.y + 10});
    REQUIRE(fixture.view.menuBar().isOpen());
    fixture.view.keyboard(EditKey::Escape);
    CHECK_FALSE(fixture.view.menuBar().isOpen());
}

TEST_CASE("LibraryView: Settings has nothing to choose yet, the language has its own menu")
{
    Fixture fixture;
    chooseEntry(fixture, 2, 0);
    CHECK(fixture.view.menuBar().isOpen()); // the only entry is disabled
    RecordingCanvas canvas;
    fixture.view.draw(canvas);
    CHECK(canvas.drew("No settings yet"));
    CHECK_FALSE(canvas.drew("Deutsch"));
}

TEST_CASE("LibraryView: View shows or hides the playlist window and tells whether it is shown")
{
    Fixture fixture;
    chooseEntry(fixture, 4, 2);
    CHECK(fixture.view.menuBar().isOpen()); // off without a playlist window
    fixture.view.keyboard(EditKey::Escape);

    bool                                      shown = false;
    std::vector<LibraryView::PlaylistCommand> commands;
    fixture.view.setPlaylistCommands([&commands](LibraryView::PlaylistCommand command) { commands.push_back(command); },
                                     [&shown] { return shown; });
    fixture.view.update();
    chooseEntry(fixture, 4, 2);
    CHECK(commands == std::vector{LibraryView::PlaylistCommand::ToggleWindow});

    shown = true;
    fixture.view.update();
    RecordingCanvas canvas;
    const Rect      title = fixture.view.menuBar().titleArea(4);
    fixture.tap({title.x + 10, title.y + 10});
    fixture.view.draw(canvas);
    CHECK(canvas.drew("Playlist window"));
}

TEST_CASE("LibraryView: View > New player calls the app and shows its shortcut")
{
    Fixture fixture;
    chooseEntry(fixture, 4, 0);
    CHECK(fixture.view.menuBar().isOpen()); // off until the app handles it
    fixture.view.keyboard(EditKey::Escape);

    int decks = 0;
    fixture.view.setOnNewDeck([&decks] { ++decks; });
    fixture.view.update();
    chooseEntry(fixture, 4, 0);
    CHECK(decks == 1);

    const Rect title = fixture.view.menuBar().titleArea(4);
    fixture.tap({title.x + 10, title.y + 10});
    RecordingCanvas canvas;
    fixture.view.draw(canvas);
    CHECK(canvas.drew("Ctrl+N"));
    CHECK(canvas.drew("New player"));
}

TEST_CASE("LibraryView: View > Crossfader window shows or hides the window and tells whether it is shown")
{
    Fixture fixture;
    chooseEntry(fixture, 4, 1);
    CHECK(fixture.view.menuBar().isOpen()); // off until the app handles it
    fixture.view.keyboard(EditKey::Escape);

    int  toggles = 0;
    bool shown   = false;
    fixture.view.setMixerWindow([&toggles] { ++toggles; }, [&shown] { return shown; });
    fixture.view.update();
    chooseEntry(fixture, 4, 1);
    CHECK(toggles == 1);

    shown = true;
    fixture.view.update();
    const Rect title = fixture.view.menuBar().titleArea(4);
    fixture.tap({title.x + 10, title.y + 10});
    RecordingCanvas canvas;
    fixture.view.draw(canvas);
    CHECK(canvas.drew("Crossfader window"));
}

TEST_CASE("LibraryView: Enter loads the selected track, like a double tap")
{
    Fixture fixture;
    fixture.view.keyboard(EditKey::Enter);
    CHECK(fixture.loaded.empty()); // nothing selected

    fixture.tap(fixture.firstRow());
    fixture.view.update();
    fixture.view.keyboard(EditKey::Enter);
    REQUIRE(fixture.loaded.size() == 1);
    CHECK(fixture.loaded[0].id == fixture.view.trackList().tracks()[0].id);
    CHECK(fixture.view.selectedTrack()->id == fixture.loaded[0].id);
}

TEST_CASE("LibraryView: Enter ends a tag edit and does not load")
{
    Fixture fixture;
    fixture.tap(fixture.firstRow());
    fixture.view.update();
    fixture.now += 1s; // a long press on the title cell starts the editor
    const Rect cell = *fixture.view.trackList().cellArea({0, api::LibraryColumn::Title});
    fixture.router.handle({{1, 1}, PointerPhase::Down, {cell.x + 5, cell.y + 5}});
    fixture.now += 2s;
    fixture.view.update();
    fixture.router.handle({{1, 1}, PointerPhase::Up, {cell.x + 5, cell.y + 5}});
    REQUIRE(fixture.view.editing());
    fixture.view.keyboard(EditKey::Enter);
    CHECK_FALSE(fixture.view.editing());
    CHECK(fixture.loaded.empty());
}

TEST_CASE("LibraryView: a notice shows in the status line for a while")
{
    Fixture fixture;
    fixture.view.setNotice("library.notice.all_playing");
    fixture.view.update();
    CHECK(fixture.view.statusText().find("all players are playing") != std::string::npos);
    fixture.now += LibraryView::notice_time + 1s;
    fixture.view.update();
    CHECK(fixture.view.statusText().find("all players are playing") == std::string::npos);
}

namespace
{
    /// Right click on a row of the list
    void rightClick(Fixture &fixture, std::size_t row)
    {
        const Rect &list = fixture.view.trackList().bounds();
        fixture.view.secondaryClick({list.x + 20, list.y + (TrackList::row_height * static_cast<float>(row)) + 10});
        fixture.view.update();
    }

    void chooseContext(Fixture &fixture, std::size_t item)
    {
        const Rect area = fixture.view.contextMenu().itemArea(item);
        fixture.tap({area.x + 10, area.y + 10});
        fixture.view.update();
    }
} // namespace

TEST_CASE("LibraryView: the right mouse button on a track selects it and opens the context menu")
{
    Fixture fixture;
    rightClick(fixture, 1);

    CHECK(fixture.view.contextMenu().isOpen());
    CHECK(fixture.view.trackList().selected() == 1U);
    CHECK(fixture.loaded.empty());

    djw::tests::RecordingCanvas canvas;
    fixture.view.draw(canvas);
    CHECK(canvas.drew("Edit tags"));
    CHECK(canvas.drew("Scan again"));
}

TEST_CASE("LibraryView: the right mouse button outside the rows or with a menu open opens no context menu")
{
    Fixture fixture;
    fixture.view.secondaryClick({5, fixture.view.searchField().bounds().y + 5});
    CHECK_FALSE(fixture.view.contextMenu().isOpen());

    const Rect title = fixture.view.menuBar().titleArea(0);
    fixture.tap({title.x + 10, title.y + 10});
    REQUIRE(fixture.view.menuBar().isOpen());
    rightClick(fixture, 0);
    CHECK_FALSE(fixture.view.contextMenu().isOpen());
}

TEST_CASE("LibraryView: Edit tags in the context menu opens the artist cell of the track in the editor")
{
    Fixture fixture; // the second track is in use in a player and cannot be edited
    rightClick(fixture, 0);
    chooseContext(fixture, 0);

    CHECK_FALSE(fixture.view.contextMenu().isOpen());
    REQUIRE(fixture.view.editing());
    CHECK(fixture.view.cellEditor().text() == fixture.view.trackList().tracks()[0].artist);
    fixture.view.keyboard(std::string{"x"});
    press(fixture, EditKey::Enter);
    REQUIRE(fixture.library.edits.size() == 1);
    CHECK(fixture.library.edits.front().first == fixture.view.trackList().tracks()[0].id);
}

TEST_CASE("LibraryView: Scan again in the context menu has the library read that track anew")
{
    Fixture fixture;
    rightClick(fixture, 2);
    chooseContext(fixture, 1);

    CHECK_FALSE(fixture.view.contextMenu().isOpen());
    CHECK(fixture.library.rescanned == std::vector<std::int64_t>{fixture.view.trackList().tracks()[2].id});
    CHECK(fixture.library.rescans == 0); // not the whole library
}

TEST_CASE("LibraryView: Analyze all tracks in the Database menu queues the analysis, the status line counts down")
{
    Fixture fixture;
    chooseEntry(fixture, 0, 2);
    CHECK(fixture.library.analyze_all_requests == 1);

    fixture.library.library_status.analysis_pending = 3;
    fixture.view.update();
    CHECK(fixture.view.statusText().find("analyzing 3 tracks") != std::string::npos);

    fixture.library.library_status.analysis_pending = 0;
    fixture.view.update();
    CHECK(fixture.view.statusText().find("analyzing") == std::string::npos);
}

TEST_CASE("LibraryView: View > Save layout is off without a handler, with one it saves and says so")
{
    Fixture fixture;
    chooseEntry(fixture, 4, 5);
    CHECK(fixture.view.menuBar().isOpen()); // disabled: stays open
    fixture.view.keyboard(EditKey::Escape);

    int saves = 0;
    fixture.view.setSaveLayout([&saves] { return ++saves == 1; });
    fixture.view.update();
    chooseEntry(fixture, 4, 5);
    CHECK(saves == 1);
    CHECK(fixture.view.statusText().find("layout saved") != std::string::npos);

    chooseEntry(fixture, 4, 5);
    CHECK(saves == 2);
    CHECK(fixture.view.statusText().find("layout could not be saved") != std::string::npos);
}

TEST_CASE("LibraryView: View offers separate windows or one window for the next start and says so")
{
    Fixture               fixture;
    constexpr std::size_t view_menu  = 4;
    constexpr std::size_t separate   = 6;
    constexpr std::size_t one_window = 7;

    // Off without a place to keep the choice: the menu stays open
    chooseEntry(fixture, view_menu, one_window);
    CHECK(fixture.view.menuBar().isOpen());
    fixture.view.keyboard(EditKey::Escape);

    WindowMode              next = WindowMode::Separate;
    std::vector<WindowMode> chosen;
    bool                    keeps = true;
    fixture.view.setWindowMode(
        [&](WindowMode mode)
        {
            chosen.push_back(mode);
            next = mode;
            return keeps;
        },
        [&next] { return next; });
    fixture.view.update();
    RecordingCanvas canvas;
    const Rect      title = fixture.view.menuBar().titleArea(view_menu);
    fixture.tap({title.x + 10, title.y + 10});
    fixture.view.draw(canvas);
    CHECK(canvas.drew("Separate windows"));
    CHECK(canvas.drew("One window"));
    fixture.view.keyboard(EditKey::Escape);

    chooseEntry(fixture, view_menu, one_window);
    chooseEntry(fixture, view_menu, separate);
    CHECK(chosen == std::vector{WindowMode::Mdi, WindowMode::Separate});
    CHECK(fixture.view.statusText().find("applies at the next start") != std::string::npos);

    keeps = false;
    chooseEntry(fixture, view_menu, one_window);
    fixture.view.update();
    CHECK(fixture.view.statusText().find("layout could not be saved") != std::string::npos);
}

TEST_CASE("LibraryView: View offers editing the layout and the automatic arrangement once the owner enables them")
{
    Fixture               fixture;
    constexpr std::size_t view_menu   = 4;
    constexpr std::size_t edit_layout = 8;
    constexpr std::size_t automatic   = 9;
    fixture.view.layout({0, 0, 600, 700}); // room for the whole View menu

    // Off without an owner (separate windows): the menu stays open
    chooseEntry(fixture, view_menu, edit_layout);
    CHECK(fixture.view.menuBar().isOpen());
    fixture.view.keyboard(EditKey::Escape);

    int  edits          = 0;
    int  automatic_runs = 0;
    bool editing        = false;
    fixture.view.setLayoutEditing(
        [&]
        {
            ++edits;
            editing = true;
        },
        [&editing] { return editing; }, [&automatic_runs] { ++automatic_runs; });
    fixture.view.update();
    RecordingCanvas canvas;
    const Rect      title = fixture.view.menuBar().titleArea(view_menu);
    fixture.tap({title.x + 10, title.y + 10});
    fixture.view.draw(canvas);
    CHECK(canvas.drew("Edit layout"));
    CHECK(canvas.drew("Arrange automatically"));
    fixture.view.keyboard(EditKey::Escape);

    chooseEntry(fixture, view_menu, edit_layout);
    CHECK(edits == 1);
    chooseEntry(fixture, view_menu, automatic);
    CHECK(automatic_runs == 1);

    // Fullscreen, with its key, checked while on
    constexpr std::size_t fullscreen_entry = 10;
    chooseEntry(fixture, view_menu, fullscreen_entry);
    CHECK(fixture.view.menuBar().isOpen()); // off without an owner
    fixture.view.keyboard(EditKey::Escape);
    bool full     = true;
    int  switches = 0;
    fixture.view.setFullscreen(
        [&]
        {
            ++switches;
            full = !full;
        },
        [&full] { return full; });
    fixture.view.update();
    fixture.tap({title.x + 10, title.y + 10});
    RecordingCanvas shown;
    fixture.view.draw(shown);
    CHECK(shown.drew("Fullscreen"));
    CHECK(shown.drew("F11"));
    fixture.view.keyboard(EditKey::Escape);
    chooseEntry(fixture, view_menu, fullscreen_entry);
    CHECK(switches == 1);
    CHECK_FALSE(full);
}
