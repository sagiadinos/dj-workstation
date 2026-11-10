// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_SDL_APP_HPP
#define DJW_PLAYER_UI_SDL_SDL_APP_HPP

#include "api/core.hpp"
#include "deck_view.hpp"
#include "pointer_router.hpp"

#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>

#include <expected>
#include <memory>
#include <string>

namespace djw::player_ui::sdl
{
    /**
     * The SDL3 window with one deck: owns SDL, the window and the renderer, runs the event loop.
     *
     * SDL resources are released in reverse order of creation by their owners, SDL itself last.
     */
    class SdlApp
    {
        public:
            static std::expected<std::unique_ptr<SdlApp>, std::string> create(core::api::Core  &core,
                                                                              core::api::DeckId deck);

            /// Runs until the window is closed
            void run();

        private:
            /// SDL_Init / SDL_Quit
            struct SdlLibrary
            {
                    SdlLibrary();
                    SdlLibrary(const SdlLibrary &)            = delete;
                    SdlLibrary &operator=(const SdlLibrary &) = delete;
                    SdlLibrary(SdlLibrary &&)                 = delete;
                    SdlLibrary &operator=(SdlLibrary &&)      = delete;
                    ~SdlLibrary();

                    bool initialized;
            };

            struct WindowDeleter
            {
                    void operator()(SDL_Window *window) const noexcept { SDL_DestroyWindow(window); }
            };

            struct RendererDeleter
            {
                    void operator()(SDL_Renderer *renderer) const noexcept { SDL_DestroyRenderer(renderer); }
            };

            SdlApp(core::api::Core &core, core::api::DeckId deck, core::api::DeckSnapshotBuffer &snapshot);

            [[nodiscard]] Rect windowArea() const;
            void               processEvents(bool &running);
            void               drawFrame();

            SdlLibrary                                     library;
            std::unique_ptr<SDL_Window, WindowDeleter>     window;
            std::unique_ptr<SDL_Renderer, RendererDeleter> renderer;
            bool                                           vsync = false;
            DeckView                                       deck_view;
            PointerRouter                                  router;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_SDL_APP_HPP
