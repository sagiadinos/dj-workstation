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
        core::api::DeckSnapshotBuffer *snapshot = core.deckSnapshot(deck);
        if (snapshot == nullptr)
        {
            return std::unexpected{std::string{"unknown deck"}};
        }

        std::unique_ptr<SdlApp> app{new SdlApp{core, deck, *snapshot}};
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

        const std::filesystem::path font = fontFile();
        auto                        deck_window =
            SdlWindow::create({.title = "djw", .width = 360, .height = 640, .vsync = true}, app->deck_view, font);
        if (!deck_window)
        {
            return std::unexpected{deck_window.error()};
        }
        app->windows.push_back(std::move(*deck_window));

        if (library != nullptr)
        {
            SdlApp *owner     = app.get();
            app->library_view = std::make_unique<LibraryView>(
                *library, app->dialogs, app->translator,
                [owner](const core::api::LibraryTrack &track) { owner->load(track); },
                [owner](const std::filesystem::path &file) { return file == owner->loaded_file; });
            app->library_view->setOnLanguage([owner](Language chosen) { owner->setLanguage(chosen); });
            app->library_view->setPlaylistCommands([owner](LibraryView::PlaylistCommand command)
                                                   { owner->runPlaylistCommand(command); });
            auto library_window = SdlWindow::create(
                {.title = app->translator.tr("window.library"), .width = 720, .height = 640}, *app->library_view, font);
            if (!library_window)
            {
                return std::unexpected{library_window.error()};
            }
            (*library_window)->placeRightOf(*app->windows.front());
            app->library_window = library_window->get();
            app->windows.push_back(std::move(*library_window));

            app->playlist_view = std::make_unique<PlaylistView>(
                *library, app->dialogs, app->translator,
                [owner](const core::api::LibraryTrack &track) { owner->load(track); });
            app->library_view->setAddToPlaylist([owner](const core::api::LibraryTrack &track)
                                                { owner->playlist_view->addTrack(track); });
            auto playlist_window = SdlWindow::create(
                {.title = app->translator.tr("window.playlist"), .width = 560, .height = 640}, *app->playlist_view, font);
            if (!playlist_window)
            {
                return std::unexpected{playlist_window.error()};
            }
            (*playlist_window)->placeRightOf(*app->windows.back());
            app->dialogs.setParent((*playlist_window)->handle());
            app->playlist_window = playlist_window->get();
            app->windows.push_back(std::move(*playlist_window));
        }
        return app;
    }

    SdlApp::SdlApp(core::api::Core &core, core::api::DeckId deck, core::api::DeckSnapshotBuffer &snapshot)
        : core{&core}, deck{deck}, deck_view{deck, core.commandQueue(), snapshot}
    {
    }

    void SdlApp::load(const core::api::LibraryTrack &track)
    {
        if (auto loaded = core->loadTrack(deck, track.path); loaded)
        {
            setDeckTrack(track.path, displayTitle(track));
        }
        else
        {
            deck_view.setTrackTitle(translator.tr("deck.cannot_load", {{"error", loaded.error()}}));
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
        if (language_changed)
        {
            language_changed(language);
        }
    }

    void SdlApp::runPlaylistCommand(LibraryView::PlaylistCommand command)
    {
        if (!playlist_view)
        {
            return;
        }
        switch (command)
        {
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
        const bool paced   = std::ranges::any_of(windows, [](const auto &window) { return window->paced(); });
        bool       running = true;
        while (running)
        {
            processEvents(running);
            dialogs.deliver();
            for (const auto &window : windows)
            {
                window->frame();
            }
            if (!paced)
            {
                SDL_Delay(frame_ms);
            }
        }
    }

    void SdlApp::processEvents(bool &running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
            {
                running = false;
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
