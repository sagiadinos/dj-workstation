// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_SDL_APP_HPP
#define DJW_PLAYER_UI_SDL_SDL_APP_HPP

#include "api/core.hpp"
#include "api/library.hpp"
#include "deck_view.hpp"
#include "i18n/translator.hpp"
#include "library_view.hpp"
#include "playlist_view.hpp"
#include "sdl_file_dialogs.hpp"
#include "sdl_window.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace djw::player_ui::sdl
{
    /**
     * The SDL3 player: a window with one deck and, if there is a library, the library window, which is the main window,
     * and a playlist window that the user opens from the Playlist or View menu. Owns SDL and SDL_ttf and runs the event
     * loop. Closing the deck or library window ends the program; closing the playlist window only closes it.
     *
     * Declaration order is destruction order in reverse: windows before the views they show, SDL itself last.
     */
    class SdlApp
    {
        public:
            /// Called with the new language when the user picks one in the Settings menu, to keep it for the next start
            using LanguageChanged = std::function<void(Language language)>;

            /// library may be nullptr: then there is no library window. The texts start in language.
            static std::expected<std::unique_ptr<SdlApp>, std::string>
            create(core::api::Core &core, core::api::DeckId deck, core::api::Library *library, Language language,
                   LanguageChanged language_changed = {});

            /// Runs until a window is closed
            void run();

            /// What the deck shows as its track, for a track loaded from outside the library
            void setDeckTrack(std::filesystem::path file, std::string title)
            {
                loaded_file = std::move(file);
                deck_view.setTrackTitle(std::move(title));
            }

        private:
            /// SDL_Init and TTF_Init, undone in reverse
            struct SdlLibraries
            {
                    SdlLibraries();
                    SdlLibraries(const SdlLibraries &)            = delete;
                    SdlLibraries &operator=(const SdlLibraries &) = delete;
                    SdlLibraries(SdlLibraries &&)                 = delete;
                    SdlLibraries &operator=(SdlLibraries &&)      = delete;
                    ~SdlLibraries();

                    bool sdl_initialized = false;
                    bool ttf_initialized = false;
            };

            SdlApp(core::api::Core &core, core::api::DeckId deck, core::api::DeckSnapshotBuffer &snapshot);

            void load(const core::api::LibraryTrack &track);
            void setLanguage(Language language);
            void runPlaylistCommand(LibraryView::PlaylistCommand command);
            void addToPlaylist(const core::api::LibraryTrack &track);
            /// The playlist window is opened and closed between frames, never while the windows are being drawn
            void openPlaylistWindow();
            void closePlaylistWindow();
            void applyWindowRequests();
            void processEvents(bool &running);

            SdlLibraries      libraries;
            SdlFileDialogs    dialogs;
            Translator        translator;
            LanguageChanged   language_changed;
            core::api::Core  *core;
            core::api::DeckId deck;
            DeckView          deck_view;
            /// The file loaded into the deck; its tags are not changed while it is loaded
            std::filesystem::path                   loaded_file;
            std::unique_ptr<LibraryView>            library_view;
            std::unique_ptr<PlaylistView>           playlist_view;
            std::vector<std::unique_ptr<SdlWindow>> windows;
            /// Window titles are set again when the language changes
            SdlWindow *library_window = nullptr;
            /// Open only while the user wants it: not at start, and closing it does not end the program
            SdlWindow *playlist_window = nullptr;

            enum class WindowRequest : std::uint8_t
            {
                None,
                OpenPlaylist,
                ClosePlaylist
            };
            WindowRequest         window_request = WindowRequest::None;
            std::filesystem::path font;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_SDL_APP_HPP
