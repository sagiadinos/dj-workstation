// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_WIDGET_HPP
#define DJW_PLAYER_UI_SDL_WIDGET_HPP

#include "canvas.hpp"
#include "geometry.hpp"

namespace djw::player_ui::sdl
{
    /**
     * Base of the custom widgets.
     *
     * A widget gets pointer events only from the pointer that captured it on down (see PointerRouter), so it never
     * has to sort out several fingers itself.
     */
    class Widget
    {
        public:
            Widget()                          = default;
            Widget(const Widget &)            = delete;
            Widget &operator=(const Widget &) = delete;
            Widget(Widget &&)                 = delete;
            Widget &operator=(Widget &&)      = delete;
            virtual ~Widget()                 = default;

            [[nodiscard]] const Rect &bounds() const noexcept { return area; }
            void                      setBounds(const Rect &rect) noexcept { area = rect; }

            virtual void pointerDown(Point position) = 0;
            virtual void pointerMove(Point position) = 0;
            /// Also called when the pointer is cancelled
            virtual void pointerUp(Point position) = 0;

            virtual void draw(Canvas &canvas) const = 0;

        private:
            Rect area;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_WIDGET_HPP
