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
        ShiftRight,
        ShiftUp,
        ShiftDown,
        /// Ctrl with the cursor keys, with or without Shift: e.g. resizing a part while the layout is edited
        CtrlLeft,
        CtrlRight,
        CtrlUp,
        CtrlDown,
        CtrlShiftLeft,
        CtrlShiftRight,
        CtrlShiftUp,
        CtrlShiftDown
    };

    /// The keys of a player, all with the left Alt (Option on the Mac) held, so typing never reaches a player: P
    /// play/pause, C cue (also when let go, since CUE acts while held), L load, the cursor keys left and right a step
    /// back and ahead (with Shift a long one), up and down the pitch by the finest step (0.02 %; with Shift a tenth of
    /// a percent).
    struct PlayerKey
    {
            enum class Key : std::uint8_t
            {
                Play,
                Cue,
                Load,
                Back,
                Ahead,
                FarBack,
                FarAhead,
                PitchUp,
                PitchDown,
                FarPitchUp,
                FarPitchDown
            };
            Key  key  = Key::Play;
            bool down = true;

            bool operator==(const PlayerKey &) const = default;
    };

    /// Typed text (UTF-8, from a physical or on-screen keyboard, after input methods), an editing key or a key of a
    /// player.
    using KeyboardInput = std::variant<std::string, EditKey, PlayerKey>;
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_KEYBOARD_INPUT_HPP
