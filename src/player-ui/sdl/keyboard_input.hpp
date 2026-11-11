// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_KEYBOARD_INPUT_HPP
#define DJW_PLAYER_UI_SDL_KEYBOARD_INPUT_HPP

#include <cstdint>
#include <string>
#include <variant>

namespace djw::player_ui::sdl
{
    /// Keys that edit text or move around, apart from typing
    enum class EditKey : std::uint8_t
    {
        Backspace,
        Enter,
        Escape,
        /// To the next field
        Tab,
        /// Shift+Tab: to the field before
        BackTab,
        Up,
        Down,
        PageUp,
        PageDown
    };

    /// Typed text (UTF-8, from a physical or on-screen keyboard, after input methods) or an editing key
    using KeyboardInput = std::variant<std::string, EditKey>;
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_KEYBOARD_INPUT_HPP
