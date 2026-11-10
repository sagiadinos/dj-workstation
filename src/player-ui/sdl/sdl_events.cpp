// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "sdl_events.hpp"

#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_touch.h>

namespace djw::player_ui::sdl
{
    namespace
    {
        std::optional<PointerEvent> fromMouseButton(const SDL_MouseButtonEvent &button, PointerPhase phase)
        {
            if (button.which == SDL_TOUCH_MOUSEID || button.button != SDL_BUTTON_LEFT)
            {
                return std::nullopt;
            }
            return PointerEvent{mouse_pointer, phase, {button.x, button.y}};
        }

        std::optional<PointerEvent> fromFinger(const SDL_TouchFingerEvent &finger, PointerPhase phase,
                                               float window_width, float window_height)
        {
            if (finger.touchID == SDL_MOUSE_TOUCHID)
            {
                return std::nullopt;
            }
            return PointerEvent{
                {finger.touchID, finger.fingerID}, phase, {finger.x * window_width, finger.y * window_height}};
        }
    } // namespace

    std::optional<PointerEvent> toPointerEvent(const SDL_Event &event, float window_width, float window_height)
    {
        switch (event.type)
        {
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                return fromMouseButton(event.button, PointerPhase::Down);
            case SDL_EVENT_MOUSE_BUTTON_UP:
                return fromMouseButton(event.button, PointerPhase::Up);
            case SDL_EVENT_MOUSE_MOTION:
                if (event.motion.which == SDL_TOUCH_MOUSEID)
                {
                    return std::nullopt;
                }
                return PointerEvent{mouse_pointer, PointerPhase::Move, {event.motion.x, event.motion.y}};
            case SDL_EVENT_FINGER_DOWN:
                return fromFinger(event.tfinger, PointerPhase::Down, window_width, window_height);
            case SDL_EVENT_FINGER_MOTION:
                return fromFinger(event.tfinger, PointerPhase::Move, window_width, window_height);
            case SDL_EVENT_FINGER_UP:
                return fromFinger(event.tfinger, PointerPhase::Up, window_width, window_height);
            case SDL_EVENT_FINGER_CANCELED:
                return fromFinger(event.tfinger, PointerPhase::Cancel, window_width, window_height);
            default:
                return std::nullopt;
        }
    }

    std::optional<KeyboardInput> toKeyboardInput(const SDL_Event &event)
    {
        if (event.type == SDL_EVENT_TEXT_INPUT && event.text.text != nullptr)
        {
            return KeyboardInput{std::string{event.text.text}};
        }
        if (event.type != SDL_EVENT_KEY_DOWN)
        {
            return std::nullopt;
        }
        switch (event.key.key)
        {
            case SDLK_BACKSPACE:
                return KeyboardInput{EditKey::Backspace};
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                return KeyboardInput{EditKey::Enter};
            case SDLK_ESCAPE:
                return KeyboardInput{EditKey::Escape};
            default:
                return std::nullopt;
        }
    }

    std::optional<WheelEvent> toWheelEvent(const SDL_Event &event)
    {
        if (event.type != SDL_EVENT_MOUSE_WHEEL)
        {
            return std::nullopt;
        }
        const float dy = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event.wheel.y : event.wheel.y;
        return WheelEvent{{event.wheel.mouse_x, event.wheel.mouse_y}, dy};
    }
} // namespace djw::player_ui::sdl
