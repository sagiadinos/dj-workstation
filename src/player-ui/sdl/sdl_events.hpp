// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_SDL_EVENTS_HPP
#define DJW_PLAYER_UI_SDL_SDL_EVENTS_HPP

#include "keyboard_input.hpp"
#include "pointer_event.hpp"

#include <SDL3/SDL_events.h>

#include <optional>
#include <string>

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

    /// Typed text, the editing and moving keys (Backspace, Enter, Escape, Tab, cursor and page keys) and the player
    /// keys (see PlayerKey), std::nullopt for all other events. Text typed while the left Alt is held is dropped: on
    /// the Mac Option with a letter types a sign, e.g. Option+P a π.
    std::optional<KeyboardInput> toKeyboardInput(const SDL_Event &event);

    /// The character a key typed, for a window that takes no typing: a letter, digit or sign as the layout and Shift
    /// make it, std::nullopt for other keys, a space, and keys with Ctrl, Alt or the system key held. It goes to the
    /// library's search, so a title typed with a player in the focus is searched for and does not reach the player.
    std::optional<std::string> strayCharacter(const SDL_Event &event);

    /// Mouse wheel turned at position; positive dy scrolls up, whatever the system's scroll direction
    struct WheelEvent
    {
            Point position;
            float dy = 0.0F;
    };

    std::optional<WheelEvent> toWheelEvent(const SDL_Event &event);

    /// Where the right mouse button went down, std::nullopt for all other events (and for touch)
    std::optional<Point> toSecondaryClick(const SDL_Event &event);
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_SDL_EVENTS_HPP
