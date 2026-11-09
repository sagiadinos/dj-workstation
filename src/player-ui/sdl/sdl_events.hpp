// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_SDL_EVENTS_HPP
#define DJW_PLAYER_UI_SDL_SDL_EVENTS_HPP

#include "pointer_event.hpp"

#include <SDL3/SDL_events.h>

#include <optional>

namespace djw::player_ui::sdl
{
    /// Pointer id of the mouse; touch devices have their own non-zero ids
    inline constexpr PointerId mouse_pointer{0, 0};

    /**
     * Turns SDL mouse and touch events into pointer events, std::nullopt for all other events.
     *
     * Mouse events that SDL synthesizes from touch (and touch events synthesized from the mouse) are dropped, so a
     * finger is never seen twice. Touch positions come normalized and are scaled to the window size in points.
     */
    std::optional<PointerEvent> toPointerEvent(const SDL_Event &event, float window_width, float window_height);
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_SDL_EVENTS_HPP
