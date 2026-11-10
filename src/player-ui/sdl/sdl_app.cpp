// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "sdl_app.hpp"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <algorithm>
#include <ranges>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr Uint32 frame_ms = 16; // fallback pacing without vsync

        /// Ctrl+N makes a new deck; the Command key on a Mac
#ifdef __APPLE__
        constexpr SDL_Keymod new_deck_modifier = SDL_KMOD_GUI;
#else
        constexpr SDL_Keymod new_deck_modifier = SDL_KMOD_CTRL;
#endif

        bool initSdl()
        {
            // Touch is handled as touch; no extra mouse events for it
            SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
            return SDL_Init(SDL_INIT_VIDEO);
        }

        std::string sdlError(const char *what)
        {
            return std::string{what} + ": " + SDL_GetError();
        }

        /// The UI font, copied next to the binary at build time
        std::filesystem::path fontFile()
        {
            const char *base = SDL_GetBasePath();
            return std::filesystem::path{base != nullptr ? base : ""} / "fonts" / "NotoSans-Regular.ttf";
        }

        std::string displayTitle(const core::api::LibraryTrack &track)
        {
            return track.artist.empty() ? track.title : track.artist + " - " + track.title;
        }
    } // namespace

    SdlApp::SdlLibraries::SdlLibraries() : sdl_initialized{initSdl()}, ttf_initialized{sdl_initialized && TTF_Init()} {}

    SdlApp::SdlLibraries::~SdlLibraries()
    {
        if (ttf_initialized)
        {
            TTF_Quit();
        }
        if (sdl_initialized)
        {
            SDL_Quit();
        }
    }

    std::expected<std::unique_ptr<SdlApp>, std::string> SdlApp::create(core::api::Core &core, core::api::DeckId deck,
                                                                       core::api::Library *library, Language language,
                                                                       LanguageChanged language_changed)
    {
        if (core.deckSnapshot(deck) == nullptr)
        {
            return std::unexpected{std::string{"unknown deck"}};
        }

        std::unique_ptr<SdlApp> app{new SdlApp{core}};
        app->translator.setLanguage(language);
        app->language_changed = std::move(language_changed);
        if (!app->libraries.sdl_initialized)
        {
            return std::unexpected{sdlError("cannot initialize SDL")};
        }
        if (!app->libraries.ttf_initialized)
        {
            return std::unexpected{sdlError("cannot initialize SDL_ttf")};
        }

        const std::filesystem::path &font = app->font = fontFile();
        if (!app->openDeckWindow(deck))
        {
            return std::unexpected{std::string{"cannot open the window of the deck"}};
        }

        if (library != nullptr)
        {
            SdlApp *owner     = app.get();
            app->library_view = std::make_unique<LibraryView>(
                *library, app->dialogs, app->translator,
                [owner](const core::api::LibraryTrack &track) { owner->load(track); },
                [owner](const std::filesystem::path &file)
                {
                    return std::ranges::any_of(owner->decks, [&file](const auto &deck_window)
                                               { return deck_window->loaded_file == file; });
                });
            app->library_view->setOnNewDeck([owner] { ++owner->new_decks_requested; });
            app->library_view->setOnLanguage([owner](Language chosen) { owner->setLanguage(chosen); });
            app->library_view->setPlaylistCommands([owner](LibraryView::PlaylistCommand command)
                                                   { owner->runPlaylistCommand(command); },
                                                   [owner] { return owner->playlist_window != nullptr; });
            auto library_window = SdlWindow::create(
                {.title = app->translator.tr("window.library"), .width = 720, .height = 640}, *app->library_view, font);
            if (!library_window)
            {
                return std::unexpected{library_window.error()};
            }
            (*library_window)->placeRightOf(*app->windows.front());
            app->library_window = library_window->get();
            app->windows.push_back(std::move(*library_window));

            // The view lives on; only its window comes and goes
            app->playlist_view = std::make_unique<PlaylistView>(
                *library, app->dialogs, app->translator,
                [owner](const core::api::LibraryTrack &track) { owner->load(track); });
            app->library_view->setAddToPlaylist([owner](const core::api::LibraryTrack &track)
                                                { owner->addToPlaylist(track); });
            app->dialogs.setParent(app->library_window->handle());
        }
        return app;
    }

    SdlApp::SdlApp(core::api::Core &core) : core{&core} {}

    SdlApp::DeckWindow *SdlApp::deckWindow(core::api::DeckId deck) noexcept
    {
        const auto found = std::ranges::find(decks, deck, &DeckWindow::id);
        return found != decks.end() ? found->get() : nullptr;
    }

    SdlApp::DeckWindow *SdlApp::deckOfWindow(SDL_WindowID window) noexcept
    {
        const auto found =
            std::ranges::find_if(decks, [window](const auto &deck) { return deck->window->id() == window; });
        return found != decks.end() ? found->get() : nullptr;
    }

    bool SdlApp::openDeckWindow(core::api::DeckId deck)
    {
        core::api::DeckSnapshotBuffer *snapshot = core->deckSnapshot(deck);
        if (snapshot == nullptr)
        {
            return false;
        }
        auto opened = std::make_unique<DeckWindow>();
        opened->id     = deck;
        opened->number = ++deck_counter;
        opened->view   = std::make_unique<DeckView>(deck, core->commandQueue(), *snapshot);
        // One window paces the frames with the display: the first one, or the next if that one is gone
        const bool pace = std::ranges::none_of(windows, [](const auto &window) { return window->paced(); });
        auto       created =
            SdlWindow::create({.title  = translator.tr("window.deck", {{"number", opened->number}}),
                               .width  = 360,
                               .height = 640,
                               .vsync  = pace},
                              *opened->view, font);
        if (!created)
        {
            SDL_Log("djw: cannot open a deck window: %s", created.error().c_str());
            return false;
        }
        if (!decks.empty())
        {
            (*created)->cascadeFrom(*decks.back()->window); // not exactly on top of the other deck
        }
        opened->window = created->get();
        windows.push_back(std::move(*created));
        decks.push_back(std::move(opened));
        active_deck = deck;
        return true;
    }

    void SdlApp::addDeck()
    {
        const core::api::DeckId deck = core->addDeck();
        if (!openDeckWindow(deck))
        {
            static_cast<void>(core->removeDeck(deck));
        }
    }

    void SdlApp::closeDeck(core::api::DeckId deck)
    {
        DeckWindow *closing = deckWindow(deck);
        if (closing == nullptr)
        {
            return;
        }
        // The window and the view go first: the view reads the deck's snapshot, which the core frees with the deck
        const SdlWindow *window = closing->window;
        std::erase_if(windows, [window](const auto &candidate) { return candidate.get() == window; });
        std::erase_if(decks, [deck](const auto &candidate) { return candidate->id == deck; });
        static_cast<void>(core->removeDeck(deck));
        if (active_deck == deck && !decks.empty())
        {
            active_deck = decks.back()->id;
        }
        // The window that paced the frames may be gone
        if (std::ranges::none_of(windows, [](const auto &candidate) { return candidate->paced(); }) && !decks.empty())
        {
            decks.front()->window->setPaced(true);
        }
    }

    void SdlApp::setDeckTitles()
    {
        for (const auto &deck : decks)
        {
            deck->window->setTitle(translator.tr("window.deck", {{"number", deck->number}}));
        }
    }

    void SdlApp::setDeckTrack(std::filesystem::path file, std::string title)
    {
        if (decks.empty())
        {
            return;
        }
        decks.front()->loaded_file = std::move(file);
        decks.front()->view->setTrackTitle(std::move(title));
    }

    void SdlApp::load(const core::api::LibraryTrack &track)
    {
        if (deckWindow(active_deck) == nullptr)
        {
            addDeck(); // every deck was closed: a track needs one
        }
        DeckWindow *target = deckWindow(active_deck);
        if (target == nullptr)
        {
            return;
        }
        if (auto loaded = core->loadTrack(target->id, track.path); loaded)
        {
            target->loaded_file = track.path;
            target->view->setTrackTitle(displayTitle(track));
        }
        else
        {
            target->view->setTrackTitle(translator.tr("deck.cannot_load", {{"error", loaded.error()}}));
        }
    }

    void SdlApp::setLanguage(Language language)
    {
        if (language == translator.language())
        {
            return;
        }
        translator.setLanguage(language);
        if (library_window != nullptr)
        {
            library_window->setTitle(translator.tr("window.library"));
        }
        if (playlist_window != nullptr)
        {
            playlist_window->setTitle(translator.tr("window.playlist"));
        }
        setDeckTitles();
        if (language_changed)
        {
            language_changed(language);
        }
    }

    void SdlApp::openPlaylistWindow()
    {
        if (!playlist_view || playlist_window != nullptr)
        {
            return;
        }
        auto created = SdlWindow::create({.title = translator.tr("window.playlist"), .width = 560, .height = 640},
                                         *playlist_view, font);
        if (!created)
        {
            SDL_Log("djw: cannot open the playlist window: %s", created.error().c_str());
            return;
        }
        if (library_window != nullptr)
        {
            (*created)->placeRightOf(*library_window);
        }
        playlist_window = created->get();
        windows.push_back(std::move(*created));
    }

    void SdlApp::closePlaylistWindow()
    {
        if (playlist_window == nullptr)
        {
            return;
        }
        std::erase_if(windows, [this](const auto &window) { return window.get() == playlist_window; });
        playlist_window = nullptr;
    }

    void SdlApp::applyWindowRequests()
    {
        for (int count = std::exchange(new_decks_requested, 0); count > 0; --count)
        {
            addDeck();
        }
        switch (std::exchange(window_request, WindowRequest::None))
        {
            case WindowRequest::OpenPlaylist:
                openPlaylistWindow();
                break;
            case WindowRequest::ClosePlaylist:
                closePlaylistWindow();
                break;
            case WindowRequest::None:
                break;
        }
    }

    void SdlApp::addToPlaylist(const core::api::LibraryTrack &track)
    {
        if (!playlist_view)
        {
            return;
        }
        if (playlist_window == nullptr)
        {
            playlist_view->update(); // reads the playlists, which the window would do on its first frame
            window_request = WindowRequest::OpenPlaylist;
        }
        playlist_view->addTrack(track);
    }

    void SdlApp::runPlaylistCommand(LibraryView::PlaylistCommand command)
    {
        if (!playlist_view)
        {
            return;
        }
        if (command == LibraryView::PlaylistCommand::ToggleWindow)
        {
            window_request = playlist_window == nullptr ? WindowRequest::OpenPlaylist : WindowRequest::ClosePlaylist;
            return;
        }
        // The commands show their result in the playlist window, so it opens if it is not
        if (playlist_window == nullptr)
        {
            playlist_view->update();
            window_request = WindowRequest::OpenPlaylist;
        }
        switch (command)
        {
            case LibraryView::PlaylistCommand::ToggleWindow:
                break;
            case LibraryView::PlaylistCommand::New:
                playlist_view->createNew();
                break;
            case LibraryView::PlaylistCommand::Import:
                playlist_view->importFile();
                break;
            case LibraryView::PlaylistCommand::Export:
                playlist_view->exportShown();
                break;
        }
    }

    void SdlApp::run()
    {
        while (runFrame())
        {
            if (std::ranges::none_of(windows, [](const auto &window) { return window->paced(); }))
            {
                SDL_Delay(frame_ms);
            }
        }
    }

    bool SdlApp::runFrame()
    {
        bool running = true;
        processEvents(running);
        if (!running || (decks.empty() && library_window == nullptr))
        {
            return false; // a quit, or the last deck window was closed and there is no library to work in
        }
        dialogs.deliver();
        for (const auto &window : windows)
        {
            window->frame();
        }
        applyWindowRequests(); // not inside the loop above, which would invalidate it
        return true;
    }

    void SdlApp::processEvents(bool &running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && playlist_window != nullptr &&
                event.window.windowID == playlist_window->id())
            {
                closePlaylistWindow(); // only this window; the program goes on
                continue;
            }
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
            {
                if (DeckWindow *closing = deckOfWindow(event.window.windowID))
                {
                    closeDeck(closing->id); // only this deck; the program goes on
                    continue;
                }
            }
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
            {
                running = false;
                continue;
            }
            if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED)
            {
                if (const DeckWindow *focused = deckOfWindow(event.window.windowID))
                {
                    active_deck = focused->id; // the next track from the library goes here
                }
            }
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && event.key.key == SDLK_N &&
                (event.key.mod & new_deck_modifier) != 0)
            {
                ++new_decks_requested; // Ctrl+N in any window
                continue;
            }
            SDL_Window *target = SDL_GetWindowFromEvent(&event);
            if (target == nullptr)
            {
                continue;
            }
            const SDL_WindowID id = SDL_GetWindowID(target);
            const auto found = std::ranges::find_if(windows, [id](const auto &window) { return window->id() == id; });
            if (found != windows.end())
            {
                (*found)->handle(event);
            }
        }
    }
} // namespace djw::player_ui::sdl
