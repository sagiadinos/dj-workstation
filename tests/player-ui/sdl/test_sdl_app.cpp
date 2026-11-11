// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "sdl_app.hpp"

#include "fake_library.hpp"
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

            std::unique_ptr<SdlApp> openApp(api::Library *library = nullptr, const std::filesystem::path &layout_file = {})
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
    Fixture fixture;
    auto    app = fixture.openApp();
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
    Fixture                fixture;
    const djw::tests::TempDir folder;
    auto                   library = api::openLibrary({.database_file = folder.path() / "library.db"});
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
    const auto                one = makeWav(folder, "one.wav");
    const auto                two = makeWav(folder, "two.wav");
    const auto                three = makeWav(folder, "three.wav");
    auto                      app = fixture.openApp();
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
    const auto                file = makeWav(folder, "one.wav");
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
    /// A WAV file of two seconds: half a second of silence, then a tone
    std::filesystem::path gapWav(const djw::tests::TempDir &folder)
    {
        djw::tests::StereoAudio audio{.sample_rate = 48'000, .samples = std::vector<float>(48'000 * 2 * 2, 0.0F)};
        for (std::size_t frame = 24'000; frame < 96'000; ++frame)
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

TEST_CASE("SdlApp: without Cue-Load a track starts at the beginning, with it at the stored cue point or the first sound")
{
    Fixture                   fixture;
    const djw::tests::TempDir folder;
    const auto                file = gapWav(folder);
    auto                      app  = fixture.openApp();
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

TEST_CASE("SdlApp: the tempo and waveform of an analyzed track show in the player, the others when their analysis is done")
{
    Fixture                   fixture;
    const djw::tests::TempDir folder;
    const auto                file = gapWav(folder);
    djw::tests::FakeLibrary   library;
    library.library_tracks.push_back({.id = 7, .path = file, .artist = "Artist", .title = "Title", .bpm = 128.0, .analyzed = true});
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
        auto                    app = fixture.openApp(&library, file); // the default: two players, the crossfader window
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
