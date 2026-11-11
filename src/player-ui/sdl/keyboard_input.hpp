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
        PageDown,
        Left,
        Right,
        /// Shift with the cursor keys: a bigger step
        ShiftLeft,
        ShiftRight
    };

    /// The keys of a player, from the original version: P play/pause, C cue, L load. Pressed without Ctrl, Alt or the
    /// system key; C also when let go, since CUE acts while held.
    struct PlayerKey
    {
            enum class Key : std::uint8_t
            {
                Play,
                Cue,
                Load
            };
            Key  key  = Key::Play;
            bool down = true;

            bool operator==(const PlayerKey &) const = default;
    };

    /// Typed text (UTF-8, from a physical or on-screen keyboard, after input methods), an editing key or a key of a
    /// player. A letter of a player key comes as both while a text field takes the typing; the views take what is
    /// theirs.
    using KeyboardInput = std::variant<std::string, EditKey, PlayerKey>;
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_KEYBOARD_INPUT_HPP
