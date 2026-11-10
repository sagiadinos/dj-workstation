// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "sdl_app.hpp"

#include "temp_dir.hpp"

#include <SDL3/SDL.h>

#include <doctest/doctest.h>

#include <memory>

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
            }

            std::unique_ptr<SdlApp> openApp(api::Library *library = nullptr)
            {
                auto app = SdlApp::create(*core, deck, library, Language::English);
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
