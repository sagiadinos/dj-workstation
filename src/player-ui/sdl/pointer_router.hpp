// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_POINTER_ROUTER_HPP
#define DJW_PLAYER_UI_SDL_POINTER_ROUTER_HPP

#include "pointer_event.hpp"
#include "widget.hpp"

#include <map>
#include <vector>

namespace djw::player_ui::sdl
{
    /**
     * Multitouch capture: every pointer captures the widget under it on down and keeps it until up.
     *
     * Moves and the up of a pointer go to its widget even outside the widget's bounds, so a fader keeps following a
     * finger that slides off it. Several fingers can work different widgets at the same time. A widget belongs to
     * one pointer at a time; a second finger on a captured widget is ignored. A pointer that went down on empty
     * space captures nothing for its whole lifetime.
     */
    class PointerRouter
    {
        public:
            /// Widgets added later lie on top. The widgets must outlive the router.
            void add(Widget &widget);

            void handle(const PointerEvent &event);

            bool isCaptured(const Widget &widget) const noexcept;

        private:
            Widget *widgetAt(Point position) const noexcept;

            std::vector<Widget *>         widgets;
            std::map<PointerId, Widget *> captures;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_POINTER_ROUTER_HPP
