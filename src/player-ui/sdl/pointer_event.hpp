// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_POINTER_EVENT_HPP
#define DJW_PLAYER_UI_SDL_POINTER_EVENT_HPP

#include "geometry.hpp"

#include <compare>
#include <cstdint>

namespace djw::player_ui::sdl
{
    /// One finger on one touch device, or the mouse. Stays the same from down to up.
    struct PointerId
    {
            std::uint64_t device = 0;
            std::uint64_t finger = 0;

            auto operator<=>(const PointerId &) const = default;
    };

    enum class PointerPhase : std::uint8_t
    {
        Down,
        Move,
        Up,
        /// The system took the pointer away (e.g. a gesture); handled like Up
        Cancel
    };

    /// Mouse and touch input in one form, so the widgets do not care where it came from
    struct PointerEvent
    {
            PointerId    pointer;
            PointerPhase phase = PointerPhase::Down;
            Point        position;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_POINTER_EVENT_HPP
