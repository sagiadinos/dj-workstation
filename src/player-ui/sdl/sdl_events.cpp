// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "sdl_events.hpp"

#include <SDL3/SDL_keyboard.h>
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

    namespace
    {
        /// A cursor key with Shift, Ctrl or both
        EditKey arrow(SDL_Keymod mod, EditKey plain, EditKey shift, EditKey ctrl, EditKey ctrl_shift)
        {
            const bool with_shift = (mod & SDL_KMOD_SHIFT) != 0;
            if ((mod & SDL_KMOD_CTRL) != 0)
            {
                return with_shift ? ctrl_shift : ctrl;
            }
            return with_shift ? shift : plain;
        }

        /// The player keys: only with the left Alt, without Ctrl, the system key or the right Alt (AltGr on many
        /// layouts types signs)
        std::optional<PlayerKey> playerKey(const SDL_KeyboardEvent &key, bool down)
        {
            constexpr SDL_Keymod others = SDL_KMOD_CTRL | SDL_KMOD_GUI | SDL_KMOD_RALT | SDL_KMOD_MODE;
            if ((key.mod & SDL_KMOD_LALT) == 0 || (key.mod & others) != 0)
            {
                return std::nullopt;
            }
            using Key        = PlayerKey::Key;
            const bool shift = (key.mod & SDL_KMOD_SHIFT) != 0;
            // Play and load once per press; CUE on press and release; the steps and the pitch repeat while held
            switch (key.key)
            {
                case SDLK_P:
                    return down && !key.repeat ? std::optional{PlayerKey{Key::Play, true}} : std::nullopt;
                case SDLK_L:
                    return down && !key.repeat ? std::optional{PlayerKey{Key::Load, true}} : std::nullopt;
                case SDLK_C:
                    return key.repeat ? std::nullopt : std::optional{PlayerKey{Key::Cue, down}};
                case SDLK_LEFT:
                    return down ? std::optional{PlayerKey{shift ? Key::FarBack : Key::Back, true}} : std::nullopt;
                case SDLK_RIGHT:
                    return down ? std::optional{PlayerKey{shift ? Key::FarAhead : Key::Ahead, true}} : std::nullopt;
                case SDLK_UP:
                    return down ? std::optional{PlayerKey{shift ? Key::FarPitchUp : Key::PitchUp, true}} : std::nullopt;
                case SDLK_DOWN:
                    return down ? std::optional{PlayerKey{shift ? Key::FarPitchDown : Key::PitchDown, true}}
                                : std::nullopt;
                default:
                    return std::nullopt;
            }
        }

        /// A code point as UTF-8
        std::string utf8(char32_t code)
        {
            std::string text;
            if (code < 0x80U)
            {
                text += static_cast<char>(code);
            }
            else if (code < 0x800U)
            {
                text += static_cast<char>(0xC0U | (code >> 6U));
                text += static_cast<char>(0x80U | (code & 0x3FU));
            }
            else if (code < 0x10000U)
            {
                text += static_cast<char>(0xE0U | (code >> 12U));
                text += static_cast<char>(0x80U | ((code >> 6U) & 0x3FU));
                text += static_cast<char>(0x80U | (code & 0x3FU));
            }
            else
            {
                text += static_cast<char>(0xF0U | (code >> 18U));
                text += static_cast<char>(0x80U | ((code >> 12U) & 0x3FU));
                text += static_cast<char>(0x80U | ((code >> 6U) & 0x3FU));
                text += static_cast<char>(0x80U | (code & 0x3FU));
            }
            return text;
        }
    } // namespace

    std::optional<KeyboardInput> toKeyboardInput(const SDL_Event &event)
    {
        if (event.type == SDL_EVENT_TEXT_INPUT && event.text.text != nullptr)
        {
            if ((SDL_GetModState() & SDL_KMOD_LALT) != 0)
            {
                return std::nullopt; // Option+letter on the Mac: a sign, not meant as typing
            }
            return KeyboardInput{std::string{event.text.text}};
        }
        if (event.type != SDL_EVENT_KEY_DOWN && event.type != SDL_EVENT_KEY_UP)
        {
            return std::nullopt;
        }
        const bool down = event.type == SDL_EVENT_KEY_DOWN;
        if (const auto player = playerKey(event.key, down))
        {
            return KeyboardInput{*player};
        }
        if (!down)
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
            case SDLK_TAB:
                return KeyboardInput{(event.key.mod & SDL_KMOD_SHIFT) != 0 ? EditKey::BackTab : EditKey::Tab};
            case SDLK_UP:
                return KeyboardInput{
                    arrow(event.key.mod, EditKey::Up, EditKey::ShiftUp, EditKey::CtrlUp, EditKey::CtrlShiftUp)};
            case SDLK_DOWN:
                return KeyboardInput{
                    arrow(event.key.mod, EditKey::Down, EditKey::ShiftDown, EditKey::CtrlDown, EditKey::CtrlShiftDown)};
            case SDLK_PAGEUP:
                return KeyboardInput{EditKey::PageUp};
            case SDLK_PAGEDOWN:
                return KeyboardInput{EditKey::PageDown};
            case SDLK_LEFT:
                return KeyboardInput{
                    arrow(event.key.mod, EditKey::Left, EditKey::ShiftLeft, EditKey::CtrlLeft, EditKey::CtrlShiftLeft)};
            case SDLK_RIGHT:
                return KeyboardInput{arrow(event.key.mod, EditKey::Right, EditKey::ShiftRight, EditKey::CtrlRight,
                                           EditKey::CtrlShiftRight)};
            default:
                return std::nullopt;
        }
    }

    std::optional<std::string> strayCharacter(const SDL_Event &event)
    {
        constexpr SDL_Keymod held = SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI;
        if (event.type != SDL_EVENT_KEY_DOWN || (event.key.mod & held) != 0)
        {
            return std::nullopt;
        }
        // The key as the layout types it with Shift and Caps Lock; printable characters are their code point
        const SDL_Keycode code = SDL_GetKeyFromScancode(event.key.scancode, event.key.mod, false);
        if (code <= SDLK_SPACE || code == SDLK_DELETE || (code & SDLK_SCANCODE_MASK) != 0 || code > 0x10FFFF)
        {
            return std::nullopt;
        }
        return utf8(static_cast<char32_t>(code));
    }

    std::optional<Point> toSecondaryClick(const SDL_Event &event)
    {
        if (event.type != SDL_EVENT_MOUSE_BUTTON_DOWN || event.button.button != SDL_BUTTON_RIGHT ||
            event.button.which == SDL_TOUCH_MOUSEID)
        {
            return std::nullopt;
        }
        return Point{event.button.x, event.button.y};
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
