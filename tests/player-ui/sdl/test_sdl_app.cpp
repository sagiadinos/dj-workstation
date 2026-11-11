// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "sdl_app.hpp"

#include "fake_library.hpp"
#include "mdi_arrangement.hpp"
#include "temp_dir.hpp"
#include "wav_file.hpp"

#include <SDL3/SDL.h>

#include <doctest/doctest.h>

#include <chrono>
#include <fstream>
#include <sstream>
#include <cmath>
#include <memory>
#include <thread>

using namespace djw::player_ui::sdl;
namespace api = djw::core::api;

namespace
{
    /// The real core without sound and SDL's dummy video driver: windows exist, nothing is shown or played
    struct Fixture
    {
            Fixture()
            {
                SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
                auto created = api::createCore({.silent_output = true});
                REQUIRE(created.has_value());
                core = std::move(*created);
                deck = core->addDeck();
                REQUIRE(core->start().has_value()); // the audio thread runs, with no sound
            }

            std::unique_ptr<SdlApp> openApp(api::Library                *library     = nullptr,
                                            const std::filesystem::path &layout_file = {})
            {
                auto app = SdlApp::create(*core, deck, library, Language::English, {}, layout_file);
                REQUIRE(app.has_value());
                return std::move(*app);
            }

            static void ctrlN()
            {
                SDL_Event event{};
                event.type       = SDL_EVENT_KEY_DOWN;
                event.key.key    = SDLK_N;
                event.key.mod    = SDL_KMOD_CTRL;
                event.key.repeat = false;
                SDL_PushEvent(&event);
            }

            static void closeWindow(SDL_WindowID window)
            {
                SDL_Event event{};
                event.type            = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
                event.window.windowID = window;
                SDL_PushEvent(&event);
            }

            std::unique_ptr<api::Core> core;
            api::DeckId                deck{};
    };
} // namespace

TEST_CASE("SdlApp: starts with the deck it is given and its window")
{
    Fixture fixture;
    auto    app = fixture.openApp();
    CHECK(app->deckCount() == 1);
    CHECK(app->runFrame());
}

TEST_CASE("SdlApp: Ctrl+N adds a deck with its own window, as many as wanted")
{
    Fixture fixture;
    auto    app = fixture.openApp();

    for (std::size_t expected = 2; expected <= 5; ++expected)
    {
        Fixture::ctrlN();
        CHECK(app->runFrame());
        CHECK(app->deckCount() == expected);
    }
    // Different windows
    CHECK(app->deckWindowId(0) != app->deckWindowId(1));
    CHECK(app->deckWindowId(3) != app->deckWindowId(4));
    // Frames keep running with several decks
    for (int i = 0; i < 5; ++i)
    {
        CHECK(app->runFrame());
    }
}

TEST_CASE("SdlApp: Ctrl+N held down does not add a deck for every repeat")
{
    Fixture   fixture;
    auto      app = fixture.openApp();
    SDL_Event event{};
    event.type       = SDL_EVENT_KEY_DOWN;
    event.key.key    = SDLK_N;
    event.key.mod    = SDL_KMOD_CTRL;
    event.key.repeat = true;
    SDL_PushEvent(&event);
    CHECK(app->runFrame());
    CHECK(app->deckCount() == 1);
}

TEST_CASE("SdlApp: N without Ctrl does nothing")
{
    Fixture   fixture;
    auto      app = fixture.openApp();
    SDL_Event event{};
    event.type    = SDL_EVENT_KEY_DOWN;
    event.key.key = SDLK_N;
    SDL_PushEvent(&event);
    CHECK(app->runFrame());
    CHECK(app->deckCount() == 1);
}

TEST_CASE("SdlApp: closing a deck window removes that deck and the others go on")
{
    Fixture fixture;
    auto    app = fixture.openApp();
    Fixture::ctrlN();
    Fixture::ctrlN();
    REQUIRE(app->runFrame());
    REQUIRE(app->deckCount() == 3);

    const SDL_WindowID second = app->deckWindowId(1);
    const SDL_WindowID third  = app->deckWindowId(2);
    Fixture::closeWindow(second);
    CHECK(app->runFrame());
    CHECK(app->deckCount() == 2);
    CHECK(app->deckWindowId(1) == third);

    // The deck is gone from the core: its snapshot cannot be asked for any more
    CHECK(app->runFrame());
    CHECK(app->runFrame());
}

TEST_CASE("SdlApp: closing the first deck window hands the frame pacing on and the app goes on")
{
    Fixture fixture;
    auto    app = fixture.openApp();
    Fixture::ctrlN();
    REQUIRE(app->runFrame());
    Fixture::closeWindow(app->deckWindowId(0));
    CHECK(app->runFrame());
    CHECK(app->deckCount() == 1);
    CHECK(app->runFrame());
}

TEST_CASE("SdlApp: without a library the program ends when the last deck window is closed")
{
    Fixture fixture;
    auto    app = fixture.openApp();
    Fixture::ctrlN();
    REQUIRE(app->runFrame());
    Fixture::closeWindow(app->deckWindowId(0));
    CHECK(app->runFrame());
    Fixture::closeWindow(app->deckWindowId(0));
    CHECK_FALSE(app->runFrame());
    CHECK(app->deckCount() == 0);
}

TEST_CASE("SdlApp: a quit event ends the program")
{
    Fixture   fixture;
    auto      app = fixture.openApp();
    SDL_Event event{};
    event.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&event);
    CHECK_FALSE(app->runFrame());
}

TEST_CASE("SdlApp: with a library the program goes on when all decks are closed and a new deck opens with Ctrl+N")
{
    Fixture                   fixture;
    const djw::tests::TempDir folder;
    auto                      library = api::openLibrary({.database_file = folder.path() / "library.db"});
    REQUIRE(library.has_value());
    auto app = fixture.openApp(library->get());

    Fixture::closeWindow(app->deckWindowId(0));
    CHECK(app->runFrame());
    CHECK(app->deckCount() == 0);
    CHECK(app->runFrame());

    Fixture::ctrlN();
    CHECK(app->runFrame());
    CHECK(app->deckCount() == 1);
}

TEST_CASE("SdlApp: the crossfader window opens on request, closing it leaves the program running")
{
    Fixture fixture;
    auto    app = fixture.openApp();
    CHECK_FALSE(app->toolWindowOpen(SdlApp::Tool::Mixer));
    CHECK(app->runFrame());
    CHECK_FALSE(app->toolWindowOpen(SdlApp::Tool::Mixer)); // not at the start

    app->requestToolWindow(SdlApp::Tool::Mixer, true);
    CHECK(app->runFrame());
    REQUIRE(app->toolWindowOpen(SdlApp::Tool::Mixer));
    CHECK(app->runFrame());

    Fixture::closeWindow(app->toolWindowId(SdlApp::Tool::Mixer));
    CHECK(app->runFrame());
    CHECK_FALSE(app->toolWindowOpen(SdlApp::Tool::Mixer));
    CHECK(app->deckCount() == 1);

    app->requestToolWindow(SdlApp::Tool::Mixer, true); // and again
    CHECK(app->runFrame());
    CHECK(app->toolWindowOpen(SdlApp::Tool::Mixer));
    app->requestToolWindow(SdlApp::Tool::Mixer, false);
    CHECK(app->runFrame());
    CHECK_FALSE(app->toolWindowOpen(SdlApp::Tool::Mixer));
}

namespace
{
    /// A library track for a WAV file of the given length
    api::LibraryTrack trackOf(const std::filesystem::path &file)
    {
        return api::LibraryTrack{.id = 1, .path = file, .artist = "Artist", .title = "Title"};
    }

    std::filesystem::path makeWav(const djw::tests::TempDir &folder, const std::string &name)
    {
        djw::tests::StereoAudio audio{.sample_rate = 48'000, .samples = std::vector<float>(48'000 * 2 * 20, 0.0F)};
        const auto              file = folder.path() / name;
        REQUIRE(djw::tests::writeWav(file, audio).has_value());
        return file;
    }

    /// Runs frames until the deck at index plays (the audio thread has to take the command first)
    bool waitForPlaying(SdlApp &app, std::size_t index, bool playing)
    {
        for (int i = 0; i < 300; ++i)
        {
            static_cast<void>(app.runFrame());
            if (app.deckPlaying(index) == playing)
            {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        }
        return false;
    }
} // namespace

TEST_CASE("SdlApp: a chosen track goes into the first player that does not play")
{
    Fixture                   fixture;
    const djw::tests::TempDir folder;
    const auto                one   = makeWav(folder, "one.wav");
    const auto                two   = makeWav(folder, "two.wav");
    const auto                three = makeWav(folder, "three.wav");
    auto                      app   = fixture.openApp();
    Fixture::ctrlN();
    Fixture::ctrlN();
    REQUIRE(app->runFrame());
    REQUIRE(app->deckCount() == 3);

    app->load(trackOf(one));
    CHECK(app->deckFile(0) == one);
    CHECK(app->deckFile(1).empty());

    // Player 1 plays now: the next track goes to player 2
    REQUIRE(fixture.core->commandQueue().tryPush(api::DeckPlay{app->deckId(0)}));
    REQUIRE(waitForPlaying(*app, 0, true));
    app->load(trackOf(two));
    CHECK(app->deckFile(0) == one); // not replaced
    CHECK(app->deckFile(1) == two);

    REQUIRE(fixture.core->commandQueue().tryPush(api::DeckPlay{app->deckId(1)}));
    REQUIRE(waitForPlaying(*app, 1, true));
    app->load(trackOf(three));
    CHECK(app->deckFile(2) == three);

    // All play: nothing is loaded
    REQUIRE(fixture.core->commandQueue().tryPush(api::DeckPlay{app->deckId(2)}));
    REQUIRE(waitForPlaying(*app, 2, true));
    app->load(trackOf(one));
    CHECK(app->deckFile(0) == one);
    CHECK(app->deckFile(1) == two);
    CHECK(app->deckFile(2) == three);

    // Player 1 stops: it is the first free one again
    REQUIRE(fixture.core->commandQueue().tryPush(api::DeckPause{app->deckId(0)}));
    REQUIRE(waitForPlaying(*app, 0, false));
    app->load(trackOf(three));
    CHECK(app->deckFile(0) == three);
    CHECK(app->deckFile(1) == two);
}

TEST_CASE("SdlApp: with a library a track chosen with no player open opens one for it")
{
    Fixture                   fixture;
    const djw::tests::TempDir folder;
    const auto                file    = makeWav(folder, "one.wav");
    auto                      library = api::openLibrary({.database_file = folder.path() / "library.db"});
    REQUIRE(library.has_value());
    auto app = fixture.openApp(library->get());

    Fixture::closeWindow(app->deckWindowId(0));
    REQUIRE(app->runFrame());
    REQUIRE(app->deckCount() == 0);

    app->load(trackOf(file));
    CHECK(app->runFrame()); // the window opens at the end of the frame, and the track follows it
    REQUIRE(app->deckCount() == 1);
    CHECK(app->deckFile(0) == file);
}

TEST_CASE("SdlApp: the track a player was started with shows as its track")
{
    Fixture fixture;
    auto    app = fixture.openApp();
    app->setDeckTrack("/music/a.mp3", "a.mp3");
    CHECK(app->deckFile(0) == "/music/a.mp3");
}

namespace
{
    /// A WAV file of seconds (two by default): half a second of silence, then a tone
    std::filesystem::path gapWav(const djw::tests::TempDir &folder, std::size_t seconds = 2)
    {
        constexpr std::size_t   rate = 48'000;
        djw::tests::StereoAudio audio{.sample_rate = rate, .samples = std::vector<float>(rate * seconds * 2, 0.0F)};
        for (std::size_t frame = rate / 2; frame < rate * seconds; ++frame)
        {
            const auto value = static_cast<float>(
                0.4 * std::sin(2.0 * 3.14159265358979 * 440.0 * static_cast<double>(frame) / 48'000.0));
            audio.samples[frame * 2]       = value;
            audio.samples[(frame * 2) + 1] = value;
        }
        const auto file = folder.path() / "gap.wav";
        REQUIRE(djw::tests::writeWav(file, audio).has_value());
        return file;
    }

    /// The position of the deck as the core reports it, after a few frames
    double positionOf(Fixture &fixture, SdlApp &app, std::size_t index)
    {
        for (int i = 0; i < 20; ++i)
        {
            static_cast<void>(app.runFrame());
            std::this_thread::sleep_for(std::chrono::milliseconds{5});
        }
        return fixture.core->deckSnapshot(app.deckId(index))->read().position_seconds;
    }
} // namespace

TEST_CASE(
    "SdlApp: without Cue-Load a track starts at the beginning, with it at the stored cue point or the first sound")
{
    Fixture                   fixture;
    const djw::tests::TempDir folder;
    // Long enough to play on until the end of the test, also on a slow machine: a player that ended is free again
    const auto file = gapWav(folder, 30);
    auto       app  = fixture.openApp();
    Fixture::ctrlN();
    Fixture::ctrlN();
    REQUIRE(app->runFrame());
    REQUIRE(app->deckCount() == 3);

    // Cue-Load off (the default): the beginning, though the track has a cue point
    api::LibraryTrack track = trackOf(file);
    track.cue_seconds       = 1.5;
    app->load(track);
    CHECK(positionOf(fixture, *app, 0) == doctest::Approx(0.0));

    // Cue-Load on, the track has a stored cue point: there
    app->setDeckCueLoad(1, true);
    REQUIRE(fixture.core->commandQueue().tryPush(api::DeckPlay{app->deckId(0)}));
    REQUIRE(waitForPlaying(*app, 0, true)); // player 1 plays, so the track goes to player 2
    app->load(track);
    CHECK(app->deckFile(1) == file);
    CHECK(positionOf(fixture, *app, 1) == doctest::Approx(1.5));

    // Cue-Load on, no stored cue point: the first sound, half a second in
    app->setDeckCueLoad(2, true);
    REQUIRE(fixture.core->commandQueue().tryPush(api::DeckPlay{app->deckId(1)}));
    REQUIRE(waitForPlaying(*app, 1, true));
    api::LibraryTrack plain = trackOf(file);
    app->load(plain);
    CHECK(positionOf(fixture, *app, 2) == doctest::Approx(0.5).epsilon(0.02));
}

TEST_CASE("SdlApp: the cue point the user sets is stored in the library for the track")
{
    Fixture                   fixture;
    const djw::tests::TempDir folder;
    const auto                file = gapWav(folder);
    djw::tests::FakeLibrary   library;
    auto                      app = fixture.openApp(&library);

    api::LibraryTrack track = trackOf(file);
    track.id                = 7;
    app->load(track);
    static_cast<void>(positionOf(fixture, *app, 0));
    CHECK(library.cue_points.empty()); // loading is no cue point the user set

    REQUIRE(fixture.core->commandQueue().tryPush(api::DeckSeek{app->deckId(0), 1.0}));
    REQUIRE(fixture.core->commandQueue().tryPush(api::DeckCue{app->deckId(0), true}));
    REQUIRE(fixture.core->commandQueue().tryPush(api::DeckCue{app->deckId(0), false}));
    for (int i = 0; i < 300 && library.cue_points.empty(); ++i)
    {
        static_cast<void>(app->runFrame());
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    REQUIRE(library.cue_points.size() == 1);
    CHECK(library.cue_points[0].first == 7);
    REQUIRE(library.cue_points[0].second.has_value());
    CHECK(*library.cue_points[0].second == doctest::Approx(1.0));
}

TEST_CASE("SdlApp: a file from outside the library has no track to store a cue point for")
{
    Fixture                   fixture;
    const djw::tests::TempDir folder;
    const auto                file = gapWav(folder);
    djw::tests::FakeLibrary   library;
    auto                      app = fixture.openApp(&library);
    app->setDeckTrack(file, "gap.wav");
    REQUIRE(fixture.core->loadTrack(app->deckId(0), file).has_value());
    static_cast<void>(positionOf(fixture, *app, 0));

    REQUIRE(fixture.core->commandQueue().tryPush(api::DeckSeek{app->deckId(0), 1.0}));
    REQUIRE(fixture.core->commandQueue().tryPush(api::DeckCue{app->deckId(0), true}));
    REQUIRE(fixture.core->commandQueue().tryPush(api::DeckCue{app->deckId(0), false}));
    for (int i = 0; i < 40; ++i)
    {
        static_cast<void>(app->runFrame());
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    CHECK(library.cue_points.empty());
}

TEST_CASE("SdlApp: a track loaded from the library is analyzed, a file from outside is not")
{
    Fixture                   fixture;
    const djw::tests::TempDir folder;
    const auto                file = gapWav(folder);
    djw::tests::FakeLibrary   library;
    auto                      app = fixture.openApp(&library);

    api::LibraryTrack track = trackOf(file);
    track.id                = 7;
    app->load(track);
    CHECK(library.analyzed == std::vector<std::int64_t>{7});

    api::LibraryTrack outside = trackOf(file); // id 0: not in the library
    REQUIRE(fixture.core->commandQueue().tryPush(api::DeckPlay{app->deckId(0)}));
    REQUIRE(waitForPlaying(*app, 0, true));
    app->load(outside);
    CHECK(library.analyzed == std::vector<std::int64_t>{7});
}

TEST_CASE(
    "SdlApp: the tempo and waveform of an analyzed track show in the player, the others when their analysis is done")
{
    Fixture                   fixture;
    const djw::tests::TempDir folder;
    const auto                file = gapWav(folder, 30); // plays on until the end of the test
    djw::tests::FakeLibrary   library;
    library.library_tracks.push_back(
        {.id = 7, .path = file, .artist = "Artist", .title = "Title", .bpm = 128.0, .analyzed = true});
    library.library_tracks.push_back({.id = 8, .path = file, .artist = "Other", .title = "Other"});
    library.waveforms[7] = {5, 100, 255};
    auto app             = fixture.openApp(&library);
    Fixture::ctrlN();
    REQUIRE(app->runFrame());
    REQUIRE(app->deckCount() == 2);

    api::LibraryTrack analyzed = trackOf(file);
    analyzed.id                = 7;
    app->load(analyzed);
    CHECK(app->deckView(0).bpm() == 128.0);
    CHECK(app->deckView(0).waveform().peaks() == std::vector<std::uint8_t>{5, 100, 255});

    // Not analyzed yet: asked for, nothing to show until the library says it is done
    REQUIRE(fixture.core->commandQueue().tryPush(api::DeckPlay{app->deckId(0)}));
    REQUIRE(waitForPlaying(*app, 0, true));
    api::LibraryTrack waiting = trackOf(file);
    waiting.id                = 8;
    app->load(waiting);
    CHECK(library.analyzed.back() == 8);
    CHECK_FALSE(app->deckView(1).bpm().has_value());
    CHECK_FALSE(app->deckView(1).waveform().hasWaveform());

    library.library_tracks.back().analyzed = true;
    library.library_tracks.back().bpm      = 90.5;
    library.waveforms[8]                   = {1, 2};
    ++library.library_status.revision;
    static_cast<void>(app->runFrame());
    CHECK(app->deckView(1).bpm() == 90.5);
    CHECK(app->deckView(1).waveform().peaks() == std::vector<std::uint8_t>{1, 2});

    // A file from outside the library takes the place of the old track: nothing of the old one stays
    api::LibraryTrack outside = trackOf(file);
    outside.id                = 0;
    app->load(outside); // player 1 plays, so it goes to player 2 again
    CHECK_FALSE(app->deckView(1).bpm().has_value());
    CHECK_FALSE(app->deckView(1).waveform().hasWaveform());
}

namespace
{
    Placement placementOfWindow(SDL_WindowID id)
    {
        SDL_Window *window = SDL_GetWindowFromID(id);
        REQUIRE(window != nullptr);
        Placement where;
        SDL_GetWindowPosition(window, &where.x, &where.y);
        SDL_GetWindowSize(window, &where.width, &where.height);
        return where;
    }
} // namespace

TEST_CASE("SdlApp: without a saved layout two players and the crossfader window open, player 1 and 2 on the sides")
{
    Fixture                   fixture;
    const djw::tests::TempDir folder;
    djw::tests::FakeLibrary   library;
    auto                      app = fixture.openApp(&library, folder.path() / "layout");

    CHECK(app->deckCount() == 2);
    CHECK(app->toolWindowOpen(SdlApp::Tool::Mixer));
    CHECK_FALSE(app->toolWindowOpen(SdlApp::Tool::Playlist));
    for (int i = 0; i < 3; ++i)
    {
        REQUIRE(app->runFrame()); // the sides reach the decks
        std::this_thread::sleep_for(std::chrono::milliseconds{20});
    }
    CHECK(app->deckView(0).crossfaderSide() == api::CrossfaderSide::A);
    CHECK(app->deckView(1).crossfaderSide() == api::CrossfaderSide::B);

    // Player 1, the crossfader window, player 2: from left to right
    const Placement first  = placementOfWindow(app->deckWindowId(0));
    const Placement mixer  = placementOfWindow(app->toolWindowId(SdlApp::Tool::Mixer));
    const Placement second = placementOfWindow(app->deckWindowId(1));
    CHECK(first.x < mixer.x);
    CHECK(mixer.x < second.x);
}

TEST_CASE("SdlApp: a saved layout brings back the players, their sides and the crossfader window")
{
    const djw::tests::TempDir folder;
    const auto                file = folder.path() / "layout";
    Layout                    saved;
    // Inside the 1024 x 768 of SDL's dummy display, so nothing is moved
    saved.players  = {{{20, 30, 380, 514}, api::CrossfaderSide::None},
                      {{120, 30, 380, 514}, api::CrossfaderSide::B},
                      {{220, 30, 380, 514}, api::CrossfaderSide::A},
                      {{320, 30, 380, 514}, api::CrossfaderSide::None},
                      {{420, 30, 380, 514}, api::CrossfaderSide::None}};
    saved.library  = {100, 100, 720, 640};
    saved.mixer    = {.open = true, .place = {300, 300, 440, 250}};
    saved.playlist = {.open = false, .place = {900, 100, 560, 640}};
    std::ofstream{file} << formatLayout(saved);

    Fixture                 fixture;
    djw::tests::FakeLibrary library;
    auto                    app = fixture.openApp(&library, file);

    CHECK(app->deckCount() == 5);
    CHECK(app->toolWindowOpen(SdlApp::Tool::Mixer));
    for (int i = 0; i < 3; ++i)
    {
        REQUIRE(app->runFrame());
        std::this_thread::sleep_for(std::chrono::milliseconds{20});
    }
    CHECK(app->deckView(1).crossfaderSide() == api::CrossfaderSide::B); // "player 2 and player 3 fade"
    CHECK(app->deckView(2).crossfaderSide() == api::CrossfaderSide::A);
    CHECK(app->deckView(0).crossfaderSide() == api::CrossfaderSide::None);
    CHECK(placementOfWindow(app->deckWindowId(2)).x == 220);
}

TEST_CASE("SdlApp: View > Save layout writes what is open now, the next start finds it again")
{
    const djw::tests::TempDir folder;
    const auto                file = folder.path() / "layout";
    {
        Fixture                 fixture;
        djw::tests::FakeLibrary library;
        auto app = fixture.openApp(&library, file); // the default: two players, the crossfader window
        REQUIRE(app->runFrame());

        Fixture::ctrlN(); // a third player
        REQUIRE(app->runFrame());
        REQUIRE(app->deckCount() == 3);
        app->requestToolWindow(SdlApp::Tool::Mixer, false);
        REQUIRE(app->runFrame());
        CHECK_FALSE(app->toolWindowOpen(SdlApp::Tool::Mixer));

        CHECK(app->saveLayout());
    }
    const auto text = [&file]
    {
        std::ifstream      in{file};
        std::ostringstream out;
        out << in.rdbuf();
        return out.str();
    }();
    const auto layout = parseLayout(text);
    REQUIRE(layout.has_value());
    CHECK(layout->players.size() == 3);
    CHECK_FALSE(layout->mixer.open);

    Fixture                 fixture;
    djw::tests::FakeLibrary library;
    auto                    app = fixture.openApp(&library, file);
    CHECK(app->deckCount() == 3);
    CHECK_FALSE(app->toolWindowOpen(SdlApp::Tool::Mixer));
}

TEST_CASE("SdlApp: a damaged layout file falls back to the default, no layout file keeps the plain start")
{
    const djw::tests::TempDir folder;
    const auto                file = folder.path() / "layout";
    std::ofstream{file} << "garbage\n";
    {
        Fixture                 fixture;
        djw::tests::FakeLibrary library;
        auto                    app = fixture.openApp(&library, file);
        CHECK(app->deckCount() == 2);
    }
    Fixture fixture;
    auto    app = fixture.openApp();
    CHECK(app->deckCount() == 1);
    CHECK_FALSE(app->toolWindowOpen(SdlApp::Tool::Mixer));
    CHECK_FALSE(app->saveLayout()); // nowhere to write
}

TEST_CASE("SdlApp: a file dropped on a player window is loaded into that player, not while it plays")
{
    Fixture                   fixture;
    const djw::tests::TempDir folder;
    const auto                file = gapWav(folder, 30); // plays on until the end of the test
    auto                      app  = fixture.openApp();
    Fixture::ctrlN();
    REQUIRE(app->runFrame());
    REQUIRE(app->deckCount() == 2);

    // Dropped on the second player: it goes there, not to the first free one
    app->dropped(app->deckWindowId(1), file);
    CHECK(app->deckFile(1) == file);
    CHECK(app->deckFile(0).empty());
    CHECK(app->deckView(1).trackTitle() == "gap"); // a file without tags shows its name

    // A playing player keeps its track
    REQUIRE(fixture.core->commandQueue().tryPush(api::DeckPlay{app->deckId(1)}));
    REQUIRE(waitForPlaying(*app, 1, true));
    const auto other = makeWav(folder, "other.wav");
    app->dropped(app->deckWindowId(1), other);
    CHECK(app->deckFile(1) == file);
    CHECK(app->deckView(1).stateText() == "not loaded"); // the player says why, where the file was dropped

    // Not a file: a folder or nothing at all is ignored
    app->dropped(app->deckWindowId(0), folder.path());
    app->dropped(app->deckWindowId(0), folder.path() / "missing.wav");
    CHECK(app->deckFile(0).empty());
}

TEST_CASE("SdlApp: the event loop turns a dropped file into a load, a folder dropped on the library is added")
{
    Fixture                   fixture;
    const djw::tests::TempDir folder;
    const auto                file = gapWav(folder);
    djw::tests::FakeLibrary   library;
    auto                      app = fixture.openApp(&library);

    const std::string path = file.string();
    SDL_Event         event{};
    event.type          = SDL_EVENT_DROP_FILE;
    event.drop.windowID = app->deckWindowId(0);
    event.drop.data     = path.c_str();
    SDL_PushEvent(&event);
    REQUIRE(app->runFrame());
    CHECK(app->deckFile(0) == file);

    const std::string folder_path = folder.path().string();
    event.drop.windowID           = app->libraryWindowId();
    event.drop.data               = folder_path.c_str();
    SDL_PushEvent(&event);
    REQUIRE(app->runFrame());
    REQUIRE(library.added_folders.size() == 1);
    CHECK(library.added_folders[0] == folder.path());
}

namespace
{
    std::string readFile(const std::filesystem::path &file)
    {
        std::ifstream      in{file};
        std::ostringstream out;
        out << in.rdbuf();
        return out.str();
    }

    /// A layout file that starts the MDI mode with players on the sides given
    void writeMdiLayout(const std::filesystem::path &file, std::vector<api::CrossfaderSide> players,
                        bool playlist_open = false)
    {
        Layout layout = defaultLayout({0, 0, 1024, 768}, {380, 514, 440, 250, 720, 640});
        layout.mode   = WindowMode::Mdi;
        layout.mdi    = MdiLayout{
            .players = std::move(players), .mixer_open = true, .playlist_open = playlist_open, .places = std::nullopt};
        std::ofstream{file} << formatLayout(layout);
    }

    int openWindows()
    {
        int          count   = 0;
        SDL_Window **windows = SDL_GetWindows(&count);
        SDL_free(static_cast<void *>(windows));
        return count;
    }

    void click(SDL_WindowID window, Point position)
    {
        SDL_Event event{};
        event.type            = SDL_EVENT_MOUSE_BUTTON_DOWN;
        event.button.windowID = window;
        event.button.which    = 1;
        event.button.button   = SDL_BUTTON_LEFT;
        event.button.down     = true;
        event.button.x        = position.x;
        event.button.y        = position.y;
        SDL_PushEvent(&event);
        event.type        = SDL_EVENT_MOUSE_BUTTON_UP;
        event.button.down = false;
        SDL_PushEvent(&event);
    }

} // namespace

TEST_CASE("SdlApp: in the MDI mode the players, the crossfader and the library are parts of one window")
{
    const djw::tests::TempDir folder;
    const auto                file = folder.path() / "layout";
    writeMdiLayout(file, {api::CrossfaderSide::A, api::CrossfaderSide::None, api::CrossfaderSide::B});

    Fixture                 fixture;
    djw::tests::FakeLibrary library;
    auto                    app = fixture.openApp(&library, file);
    CHECK(app->windowMode() == WindowMode::Mdi);
    CHECK(openWindows() == 1);
    REQUIRE(app->deckCount() == 3);
    CHECK(app->toolWindowOpen(SdlApp::Tool::Mixer));
    CHECK(app->deckWindowId(0) == app->libraryWindowId());
    CHECK(app->deckWindowId(2) == app->libraryWindowId());
    for (int i = 0; i < 3; ++i)
    {
        REQUIRE(app->runFrame());
        std::this_thread::sleep_for(std::chrono::milliseconds{20});
    }
    CHECK(app->deckView(0).crossfaderSide() == api::CrossfaderSide::A);
    CHECK(app->deckView(2).crossfaderSide() == api::CrossfaderSide::B);

    // Every player has its part of the window, side by side
    std::vector<Rect> parts;
    for (std::size_t deck = 0; deck < app->deckCount(); ++deck)
    {
        REQUIRE(app->deckFrame(deck) != nullptr);
        const auto area = app->paneArea(*app->deckFrame(deck));
        REQUIRE(area.has_value());
        CHECK(area->width > 0);
        parts.push_back(*area);
    }
    CHECK_FALSE(overlap(parts[0], parts[1]));
    CHECK_FALSE(overlap(parts[1], parts[2]));
    CHECK(parts[0].x < parts[1].x);

    // A new player is a new part, not a new window
    Fixture::ctrlN();
    REQUIRE(app->runFrame());
    CHECK(app->deckCount() == 4);
    CHECK(openWindows() == 1);
}

TEST_CASE("SdlApp: in the MDI mode the close button of a player removes it, closing the window ends the program")
{
    const djw::tests::TempDir folder;
    const auto                file = folder.path() / "layout";
    writeMdiLayout(file, {api::CrossfaderSide::A, api::CrossfaderSide::B});

    Fixture                 fixture;
    djw::tests::FakeLibrary library;
    auto                    app = fixture.openApp(&library, file);
    REQUIRE(app->runFrame()); // lays the parts out
    REQUIRE(app->deckCount() == 2);
    const api::DeckId second = app->deckId(1);

    const Rect button = app->deckFrame(0)->closeButton().bounds();
    REQUIRE(button.width > 0);
    click(app->libraryWindowId(), {button.x + (button.width / 2), button.y + (button.height / 2)});
    REQUIRE(app->runFrame());
    REQUIRE(app->deckCount() == 1);
    CHECK(app->deckId(0) == second);
    CHECK(openWindows() == 1);

    // The crossfader closes the same way and opens again from the menu
    app->requestToolWindow(SdlApp::Tool::Mixer, false);
    REQUIRE(app->runFrame());
    CHECK_FALSE(app->toolWindowOpen(SdlApp::Tool::Mixer));
    app->requestToolWindow(SdlApp::Tool::Mixer, true);
    REQUIRE(app->runFrame());
    CHECK(app->toolWindowOpen(SdlApp::Tool::Mixer));
    CHECK(openWindows() == 1);

    Fixture::closeWindow(app->libraryWindowId());
    CHECK_FALSE(app->runFrame());
}

TEST_CASE("SdlApp: in the MDI mode a file dropped on a player part goes into that player")
{
    const djw::tests::TempDir folder;
    const auto                file  = gapWav(folder);
    const auto                saved = folder.path() / "layout";
    writeMdiLayout(saved, {api::CrossfaderSide::A, api::CrossfaderSide::B});

    Fixture                 fixture;
    djw::tests::FakeLibrary library;
    auto                    app = fixture.openApp(&library, saved);
    REQUIRE(app->runFrame());
    const auto second = app->paneArea(*app->deckFrame(1));
    REQUIRE(second.has_value());

    app->dropped(app->libraryWindowId(), file); // no position: nothing
    CHECK(app->deckFile(1).empty());
    app->dropped(app->libraryWindowId(), file, Point{second->x + 20, second->y + 100});
    CHECK(app->deckFile(1) == file);
    CHECK(app->deckFile(0).empty());
}

TEST_CASE("SdlApp: the window mode chosen applies at the next start, each mode keeps its own layout")
{
    const djw::tests::TempDir folder;
    const auto                file = folder.path() / "layout";
    {
        Fixture                 fixture;
        djw::tests::FakeLibrary library;
        auto                    app = fixture.openApp(&library, file); // separate windows, two players
        REQUIRE(app->runFrame());
        Fixture::ctrlN();
        REQUIRE(app->runFrame());
        REQUIRE(app->saveLayout()); // three separate players

        CHECK(app->chooseWindowMode(WindowMode::Mdi));
        CHECK(app->windowMode() == WindowMode::Separate); // not now
        CHECK(app->nextWindowMode() == WindowMode::Mdi);
        const auto layout = parseLayout(readFile(file));
        REQUIRE(layout.has_value());
        CHECK(layout->mode == WindowMode::Mdi);
        CHECK(layout->players.size() == 3);
    }
    {
        Fixture                 fixture;
        djw::tests::FakeLibrary library;
        auto                    app = fixture.openApp(&library, file);
        CHECK(app->windowMode() == WindowMode::Mdi);
        CHECK(openWindows() == 1);
        CHECK(app->deckCount() == 2); // the one window was not saved yet: its default
        REQUIRE(app->runFrame());
        app->requestToolWindow(SdlApp::Tool::Playlist, true);
        REQUIRE(app->runFrame());
        REQUIRE(app->saveLayout());
        CHECK(app->chooseWindowMode(WindowMode::Separate));

        const auto layout = parseLayout(readFile(file));
        REQUIRE(layout.has_value());
        CHECK(layout->mode == WindowMode::Separate);
        CHECK(layout->players.size() == 3); // the separate windows as they were saved
        REQUIRE(layout->mdi.has_value());
        CHECK(layout->mdi->players.size() == 2);
        CHECK(layout->mdi->playlist_open);
    }
    Fixture                 fixture;
    djw::tests::FakeLibrary library;
    auto                    app = fixture.openApp(&library, file);
    CHECK(app->windowMode() == WindowMode::Separate);
    CHECK(app->deckCount() == 3);
    CHECK(app->deckWindowId(0) != app->deckWindowId(1));
}

TEST_CASE("SdlApp: without a layout file the window mode cannot be kept")
{
    Fixture fixture;
    auto    app = fixture.openApp();
    CHECK_FALSE(app->chooseWindowMode(WindowMode::Mdi));
    CHECK(app->windowMode() == WindowMode::Separate);
}

TEST_CASE("SdlApp: in the MDI mode the parts can be arranged by hand, keep their places and are saved with them")
{
    const djw::tests::TempDir folder;
    const auto                file = folder.path() / "layout";
    writeMdiLayout(file, {api::CrossfaderSide::A, api::CrossfaderSide::B});

    Fixture                 fixture;
    djw::tests::FakeLibrary library;
    auto                    app = fixture.openApp(&library, file);
    REQUIRE(app->runFrame());
    CHECK_FALSE(app->handPlaces().has_value()); // arranged automatically
    const auto before = app->paneArea(*app->deckFrame(0));
    REQUIRE(before.has_value());

    // Edit mode: the first player made smaller at its lower right corner
    app->startArranging();
    CHECK(app->arranging());
    ViewHost   *views = app->hostViews();
    const Point corner{before->x + before->width - 5, before->y + before->height - 5};
    const Point to{corner.x - 100, corner.y - 100};
    views->pointer({{1, 1}, PointerPhase::Down, corner});
    views->pointer({{1, 1}, PointerPhase::Move, to});
    views->pointer({{1, 1}, PointerPhase::Up, to});
    REQUIRE(app->runFrame());
    REQUIRE(app->handPlaces().has_value());
    const auto smaller = app->paneArea(*app->deckFrame(0));
    REQUIRE(smaller.has_value());
    CHECK(smaller->width < before->width - 80);
    CHECK(std::fmod(smaller->x + smaller->width, mdi_grid) == doctest::Approx(0.0F)); // on the grid
    // The others stay where they were
    REQUIRE(app->handPlaces()->players.size() == 2);
    CHECK(app->handPlaces()->players[1].has_value());

    // Done ends it; with the crossfader closed (it keeps its place) a new player takes the free place
    views->keyboard(EditKey::Escape);
    CHECK_FALSE(app->arranging());
    app->requestToolWindow(SdlApp::Tool::Mixer, false);
    REQUIRE(app->runFrame());
    REQUIRE(app->handPlaces()->mixer.has_value());
    Fixture::ctrlN();
    REQUIRE(app->runFrame());
    REQUIRE(app->runFrame()); // it came after the parts were placed; now it has its place
    REQUIRE(app->deckCount() == 3);
    REQUIRE(app->handPlaces()->players.size() == 3);
    CHECK(app->handPlaces()->players[2].has_value());
    std::vector<Rect> parts;
    for (std::size_t deck = 0; deck < app->deckCount(); ++deck)
    {
        parts.push_back(*app->paneArea(*app->deckFrame(deck)));
    }
    parts.push_back(*app->paneArea(*app->libraryFrame()));
    for (std::size_t first = 0; first < parts.size(); ++first)
    {
        for (std::size_t second = first + 1; second < parts.size(); ++second)
        {
            INFO(first << " and " << second);
            CHECK_FALSE(overlap(parts[first], parts[second]));
        }
    }

    // Saved: the next start reads the same places (not started here: a second SDL start in the MDI mode leaks
    // inside some GPU drivers)
    REQUIRE(app->saveLayout());
    const auto saved = parseLayout(readFile(file));
    REQUIRE(saved.has_value());
    REQUIRE(saved->mdi.has_value());
    CHECK(saved->mdi->places == app->handPlaces());

    // The crossfader opens again; its place is taken by the new player and the window is full: it goes on top at
    // the upper left, for the user to make room, and keeps that place
    const Placement old_mixer = app->handPlaces()->mixer.value_or(Placement{});
    app->requestToolWindow(SdlApp::Tool::Mixer, true);
    REQUIRE(app->runFrame());
    REQUIRE(app->runFrame());
    const auto mixer = app->paneArea(*app->toolFrame(SdlApp::Tool::Mixer));
    REQUIRE(mixer.has_value());
    CHECK(mixer->x == doctest::Approx(0));
    CHECK(mixer->y == doctest::Approx(0));
    REQUIRE(app->handPlaces()->mixer.has_value());
    CHECK(*app->handPlaces()->mixer != old_mixer);

    // Closing a player takes its place with it; Arrange automatically forgets them all
    const auto last  = app->handPlaces()->players[2];
    const Rect close = app->deckFrame(1)->closeButton().bounds();
    click(app->libraryWindowId(), {close.x + (close.width / 2), close.y + (close.height / 2)});
    REQUIRE(app->runFrame());
    REQUIRE(app->handPlaces()->players.size() == 2);
    CHECK(app->handPlaces()->players[1] == last);
    app->arrangeAutomatically();
    REQUIRE(app->runFrame());
    CHECK_FALSE(app->handPlaces().has_value());
}

TEST_CASE("SdlApp: in the MDI mode a start with places set by hand puts the parts there")
{
    const djw::tests::TempDir folder;
    const auto                file   = folder.path() / "layout";
    Layout                    layout = defaultLayout({0, 0, 1024, 768}, {380, 514, 440, 250, 720, 640});
    layout.mode                      = WindowMode::Mdi;
    layout.mdi = MdiLayout{.players       = {api::CrossfaderSide::A, api::CrossfaderSide::B},
                           .mixer_open    = false,
                           .playlist_open = false,
                           .places = MdiPlaces{.players  = {Placement{600, 0, 300, 400}, Placement{0, 0, 300, 400}},
                                               .mixer    = std::nullopt,
                                               .playlist = std::nullopt,
                                               .library  = Placement{0, 420, 900, 300}}};
    std::ofstream{file} << formatLayout(layout);

    Fixture                 fixture;
    djw::tests::FakeLibrary library;
    auto                    app = fixture.openApp(&library, file);
    REQUIRE(app->runFrame());
    REQUIRE(app->deckCount() == 2);
    const auto first        = app->paneArea(*app->deckFrame(0));
    const auto second       = app->paneArea(*app->deckFrame(1));
    const auto library_part = app->paneArea(*app->libraryFrame());
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    REQUIRE(library_part.has_value());
    CHECK(first->x == doctest::Approx(600));
    CHECK(first->width == doctest::Approx(300));
    CHECK(second->x == doctest::Approx(0));
    CHECK(library_part->y == doctest::Approx(420));
    CHECK(library_part->height == doctest::Approx(300));
}
