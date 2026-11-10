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

#include <SDL3/SDL_video.h>

#include <cstddef>
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
     * The SDL3 player: a window for each deck and, if there is a library, the library window, which is the main window,
     * and a playlist window that the user opens from the Playlist or View menu. Owns SDL and SDL_ttf and runs the event
     * loop.
     *
     * Decks: there is no upper limit. View > New deck or Ctrl+N adds a deck with its own window; closing a deck window
     * removes that deck. A track chosen in the library or the playlist goes to the deck used last (the window that
     * had the focus last, or the newest). Closing the library window ends the program, as does closing the last deck
     * window when there is no library; closing the playlist window only closes it.
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

            /// Runs until the program is to end (see above)
            void run();
            /// One pass of the event loop: handles the events, updates and draws the windows. False when the program
            /// is to end. run() is this in a loop; tests call it to step through events.
            bool runFrame();

            /// What the first deck shows as its track, for a track loaded from outside the library
            void setDeckTrack(std::filesystem::path file, std::string title);

            /// Number of decks with a window
            [[nodiscard]] std::size_t deckCount() const noexcept { return decks.size(); }
            /// The SDL id of the window of the deck at index (in the order they were opened); for tests
            [[nodiscard]] SDL_WindowID deckWindowId(std::size_t index) const { return decks.at(index)->window->id(); }

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

            /// A deck and its window
            struct DeckWindow
            {
                    core::api::DeckId     id;
                    int                   number = 0; ///< shown in the title
                    std::unique_ptr<DeckView> view;
                    SdlWindow            *window = nullptr; ///< owned by windows
                    /// The file loaded into the deck; its tags are not changed while it is loaded
                    std::filesystem::path loaded_file;
            };

            explicit SdlApp(core::api::Core &core);

            /// Opens the window of a deck the core has; false (with a message in the log) if that fails
            bool openDeckWindow(core::api::DeckId deck);
            /// Adds a deck to the core and opens its window
            void addDeck();
            /// Closes the window of a deck, then removes the deck from the core
            void closeDeck(core::api::DeckId deck);
            [[nodiscard]] DeckWindow *deckWindow(core::api::DeckId deck) noexcept;
            [[nodiscard]] DeckWindow *deckOfWindow(SDL_WindowID window) noexcept;
            void                      setDeckTitles();

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
            /// Views come before the windows that show them: destroyed after them
            std::vector<std::unique_ptr<DeckWindow>> decks;
            int                                      deck_counter = 0;
            /// The deck that gets tracks from the library: the one used last
            core::api::DeckId                        active_deck{};
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
            /// Asked for from a menu or the keyboard; done between frames
            int                   new_decks_requested = 0;
            std::filesystem::path font;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_SDL_APP_HPP
