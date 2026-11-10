// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_SDL_WINDOW_HPP
#define DJW_PLAYER_UI_SDL_SDL_WINDOW_HPP

#include "pointer_router.hpp"
#include "sdl_text.hpp"
#include "view.hpp"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>

#include <expected>
#include <filesystem>
#include <memory>
#include <string>

namespace djw::player_ui::sdl
{
    struct WindowOptions
    {
            std::string title;
            int         width  = 360;
            int         height = 640;
            /// Wait for the display on present; one window paces the frames, the others must not wait as well
            bool vsync = false;
    };

    /**
     * One SDL window with its renderer, its text and the view it shows. Input events of the window go to the view's
     * widgets through its own PointerRouter, typed text to the view.
     *
     * Resources are released in reverse order of creation: text, renderer, window.
     */
    class SdlWindow
    {
        public:
            /// The view must outlive the window
            static std::expected<std::unique_ptr<SdlWindow>, std::string>
            create(const WindowOptions &options, View &view, const std::filesystem::path &font_file);

            [[nodiscard]] SDL_WindowID id() const noexcept { return SDL_GetWindowID(window.get()); }
            [[nodiscard]] bool         paced() const noexcept { return vsync; }
            /// The SDL window, e.g. as parent of a dialog
            [[nodiscard]] SDL_Window *handle() const noexcept { return window.get(); }

            void setTitle(const std::string &title) const { SDL_SetWindowTitle(window.get(), title.c_str()); }

            /// Moves this window next to other, to its right, with the same top edge
            void placeRightOf(const SdlWindow &other);
            /// Moves this window a little down and to the right of other, so both can be told apart
            void cascadeFrom(const SdlWindow &other);
            /// Makes this window the one that waits for the display (vsync) or not
            void setPaced(bool paced);

            /// An input event of this window
            void handle(const SDL_Event &event);
            /// Lays out, updates and draws the view
            void frame();

        private:
            struct WindowDeleter
            {
                    void operator()(SDL_Window *window) const noexcept { SDL_DestroyWindow(window); }
            };

            struct RendererDeleter
            {
                    void operator()(SDL_Renderer *renderer) const noexcept { SDL_DestroyRenderer(renderer); }
            };

            explicit SdlWindow(View &view) noexcept : view{&view} {}

            /// Window size in points
            [[nodiscard]] Rect area() const;
            void               syncTextInput();

            View                                          *view;
            std::unique_ptr<SDL_Window, WindowDeleter>     window;
            std::unique_ptr<SDL_Renderer, RendererDeleter> renderer;
            std::unique_ptr<SdlText>                       text;
            PointerRouter                                  router;
            bool                                           vsync      = false;
            bool                                           text_input = false;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_SDL_WINDOW_HPP
