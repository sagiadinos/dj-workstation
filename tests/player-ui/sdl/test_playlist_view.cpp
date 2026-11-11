// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "playlist_view.hpp"

#include "fake_dialogs.hpp"
#include "fake_library.hpp"
#include "recording_canvas.hpp"

#include <doctest/doctest.h>

using namespace djw::player_ui::sdl;
namespace api = djw::core::api;
using namespace std::chrono_literals;
using Button = PlaylistView::Button;

namespace
{
    using djw::tests::FakeDialogs;

    struct Fixture
    {
            Fixture()
            {
                view.layout({0, 0, 560, 640});
                view.addTo(router);
                view.update();
            }

            void tap(Point position)
            {
                router.handle({{1, 1}, PointerPhase::Down, position});
                router.handle({{1, 1}, PointerPhase::Up, position});
            }

            void press(Button which)
            {
                const Rect &area = view.button(which).bounds();
                tap({area.x + (area.width / 2), area.y + (area.height / 2)});
                view.update();
            }

            void tapRow(std::size_t row)
            {
                const Rect &area = view.trackList().bounds();
                tap({area.x + 20, area.y + (TrackList::row_height * static_cast<float>(row)) + 10});
                view.update();
            }

            [[nodiscard]] std::vector<std::string> titles() const
            {
                std::vector<std::string> result;
                for (const auto &track : view.trackList().tracks())
                {
                    result.push_back(track.title);
                }
                return result;
            }

            djw::tests::FakeLibrary               library;
            FakeDialogs                           dialogs;
            Translator                            translator;
            std::vector<api::LibraryTrack>        loaded;
            std::chrono::steady_clock::time_point now{};
            PlaylistView  view{library, dialogs, translator, [this](const api::LibraryTrack &track)
                               { loaded.push_back(track); }, [this] { return now; }};
            PointerRouter router;
    };
} // namespace

TEST_CASE("PlaylistView: without playlists there is nothing to show, New creates one and shows it")
{
    Fixture fixture;
    CHECK_FALSE(fixture.view.current().has_value());
    CHECK(fixture.view.statusText() == "no playlist");

    fixture.press(Button::New);
    fixture.view.update();

    REQUIRE(fixture.view.current().has_value());
    CHECK(fixture.view.current()->name == "Playlist 1");
    CHECK(fixture.view.nameField().text() == "Playlist 1");

    fixture.press(Button::New);
    fixture.view.update();
    CHECK(fixture.view.current()->name == "Playlist 2");
}

TEST_CASE("PlaylistView: shows the first playlist with its tracks in order, < and > step through them")
{
    Fixture fixture;
    fixture.library.addPlaylist("B Set", {3, 1});
    fixture.library.addPlaylist("A Set", {2});
    ++fixture.library.library_status.revision;
    fixture.view.update();

    CHECK(fixture.view.current()->name == "A Set");
    CHECK(fixture.titles() == std::vector<std::string>{"beta"});

    fixture.press(Button::Next);
    CHECK(fixture.view.current()->name == "B Set");
    CHECK(fixture.titles() == std::vector<std::string>{"gamma", "alpha"});
    CHECK(fixture.view.statusText() == "2 tracks");

    fixture.press(Button::Next); // wraps around
    CHECK(fixture.view.current()->name == "A Set");
    fixture.press(Button::Previous);
    CHECK(fixture.view.current()->name == "B Set");
}

TEST_CASE("PlaylistView: Up and Down move the selected entry and the selection follows, Remove takes it out")
{
    Fixture fixture;
    fixture.library.addPlaylist("Set", {1, 2, 3});
    ++fixture.library.library_status.revision;
    fixture.view.update();

    fixture.tapRow(2);
    fixture.press(Button::Up);
    fixture.view.update();
    CHECK(fixture.titles() == std::vector<std::string>{"alpha", "gamma", "beta"});
    CHECK(fixture.view.trackList().selected() == 1U);

    fixture.press(Button::Up);
    fixture.press(Button::Up); // already at the top
    fixture.view.update();
    CHECK(fixture.titles() == std::vector<std::string>{"gamma", "alpha", "beta"});
    CHECK(fixture.view.trackList().selected() == 0U);

    fixture.press(Button::Down);
    fixture.view.update();
    CHECK(fixture.titles() == std::vector<std::string>{"alpha", "gamma", "beta"});

    fixture.press(Button::Remove);
    fixture.view.update();
    CHECK(fixture.titles() == std::vector<std::string>{"alpha", "beta"});
}

TEST_CASE("PlaylistView: the name field renames on Enter, Escape keeps the old name")
{
    Fixture fixture;
    fixture.library.addPlaylist("Old", {});
    ++fixture.library.library_status.revision;
    fixture.view.update();
    const Rect &field = fixture.view.nameField().bounds();

    fixture.tap({field.x + 10, field.y + 10});
    fixture.view.update();
    CHECK(fixture.view.wantsTextInput());
    fixture.view.keyboard(std::string{"er"});
    fixture.view.keyboard(EditKey::Escape);
    CHECK(fixture.view.nameField().text() == "Old");

    fixture.tap({field.x + 10, field.y + 10});
    fixture.view.update();
    fixture.view.keyboard(std::string{" House"});
    fixture.view.keyboard(EditKey::Enter);
    fixture.view.update();

    REQUIRE(fixture.library.renames.size() == 1);
    CHECK(fixture.library.renames.front().second == "Old House");
    CHECK(fixture.view.current()->name == "Old House");
    CHECK_FALSE(fixture.view.wantsTextInput());
}

TEST_CASE("PlaylistView: Delete needs a second tap in time")
{
    Fixture fixture;
    fixture.library.addPlaylist("Set", {});
    ++fixture.library.library_status.revision;
    fixture.view.update();

    fixture.press(Button::Delete);
    CHECK(fixture.view.button(Button::Delete).label() == "Really delete?");
    fixture.now += PlaylistView::confirm_time + 1ms;
    fixture.view.update();
    CHECK(fixture.view.button(Button::Delete).label() == "Delete");
    CHECK(fixture.view.current().has_value());

    fixture.press(Button::Delete);
    fixture.press(Button::Delete);
    fixture.view.update();
    CHECK_FALSE(fixture.view.current().has_value());
}

TEST_CASE("PlaylistView: Import goes through the open dialog and shows the imported playlist with its outcome")
{
    Fixture fixture;
    fixture.library.addPlaylist("Other", {});
    ++fixture.library.library_status.revision;
    fixture.view.update();

    fixture.press(Button::Import);
    CHECK(fixture.dialogs.opened);
    fixture.dialogs.choose("/lists/Friday.m3u8");
    fixture.view.update();

    CHECK(fixture.library.imports == std::vector<std::filesystem::path>{"/lists/Friday.m3u8"});
    CHECK(fixture.view.current()->name == "Friday");
    CHECK(fixture.view.statusText() == "1 track - imported Friday: 1 track, 2 not found");
}

TEST_CASE("PlaylistView: Export suggests the playlist name and writes where the user chose")
{
    Fixture    fixture;
    const auto id = fixture.library.addPlaylist("Friday", {1});
    ++fixture.library.library_status.revision;
    fixture.view.update();

    fixture.press(Button::Export);
    CHECK(fixture.dialogs.suggested == "Friday.m3u8");
    fixture.dialogs.choose("/out/Friday.m3u8");

    REQUIRE(fixture.library.exports.size() == 1);
    CHECK(fixture.library.exports.front().first == id);
    CHECK(fixture.library.exports.front().second == std::filesystem::path{"/out/Friday.m3u8"});
}

TEST_CASE("PlaylistView: tracks from the library are appended, without a playlist that is a message")
{
    Fixture fixture;
    fixture.view.addTrack(fixture.library.library_tracks[1]);
    fixture.view.update();
    CHECK(fixture.view.statusText().find("create a playlist first") != std::string::npos);

    fixture.library.addPlaylist("Set", {1});
    ++fixture.library.library_status.revision;
    fixture.view.update();
    fixture.view.addTrack(fixture.library.library_tracks[1]);
    fixture.view.update();

    CHECK(fixture.titles() == std::vector<std::string>{"alpha", "beta"});
}

TEST_CASE("PlaylistView: a double tap loads a track, errors of the library show in the status line")
{
    Fixture fixture;
    fixture.library.addPlaylist("Set", {3});
    ++fixture.library.library_status.revision;
    fixture.view.update();

    fixture.tapRow(0);
    fixture.now += 100ms;
    fixture.tapRow(0);

    REQUIRE(fixture.loaded.size() == 1);
    CHECK(fixture.loaded.front().title == "gamma");

    fixture.library.library_status.write_error = "disk full";
    fixture.view.update();
    CHECK(fixture.view.statusText() == "1 track - disk full");

    djw::tests::RecordingCanvas canvas;
    fixture.view.draw(canvas);
    CHECK(canvas.drew("gamma"));
    CHECK(canvas.drew("Export"));
}
