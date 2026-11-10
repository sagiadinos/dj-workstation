// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_SDL_APP_HPP
#define DJW_PLAYER_UI_SDL_SDL_APP_HPP

#include "api/core.hpp"
#include "api/library.hpp"
#include "deck_view.hpp"
#include "library_view.hpp"
#include "sdl_window.hpp"

#include <expected>
#include <memory>
#include <string>
#include <vector>

namespace djw::player_ui::sdl
{
    /**
     * The SDL3 player: a window with one deck and, if there is a library, a library window. Owns SDL and SDL_ttf
     * and runs the event loop. Closing any window ends the program.
     *
     * Declaration order is destruction order in reverse: windows before the views they show, SDL itself last.
     */
    class SdlApp
    {
        public:
            /// library may be nullptr: then there is no library window
            static std::expected<std::unique_ptr<SdlApp>, std::string>
            create(core::api::Core &core, core::api::DeckId deck, core::api::Library *library);

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
            void processEvents(bool &running);

            SdlLibraries      libraries;
            core::api::Core  *core;
            core::api::DeckId deck;
            DeckView          deck_view;
            /// The file loaded into the deck; its tags are not changed while it is loaded
            std::filesystem::path                   loaded_file;
            std::unique_ptr<LibraryView>            library_view;
            std::vector<std::unique_ptr<SdlWindow>> windows;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_SDL_APP_HPP
