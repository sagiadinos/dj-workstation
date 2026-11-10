// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_VIEW_HPP
#define DJW_PLAYER_UI_SDL_VIEW_HPP

#include "canvas.hpp"
#include "geometry.hpp"
#include "keyboard_input.hpp"
#include "pointer_router.hpp"

namespace djw::player_ui::sdl
{
    /// The content of one window: lays out and draws its widgets and turns their input into actions.
    class View
    {
        public:
            View()                        = default;
            View(const View &)            = delete;
            View &operator=(const View &) = delete;
            View(View &&)                 = delete;
            View &operator=(View &&)      = delete;
            virtual ~View()               = default;

            virtual void layout(const Rect &area)     = 0;
            virtual void addTo(PointerRouter &router) = 0;
            /// Once per frame, before drawing
            virtual void update()                   = 0;
            virtual void draw(Canvas &canvas) const = 0;

            /// True while the view takes typed text, so the window turns text input (and on-screen keyboards) on
            [[nodiscard]] virtual bool wantsTextInput() const { return false; }
            virtual void               keyboard(const KeyboardInput               &/*input*/) {}
            /// Mouse wheel over position; positive dy scrolls up
            virtual void wheel(Point /*position*/, float /*dy*/) {}
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_VIEW_HPP
