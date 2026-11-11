// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_PLAYER_NUMBER_HPP
#define DJW_PLAYER_UI_SDL_PLAYER_NUMBER_HPP

#include <string>

namespace djw::player_ui::sdl
{
    /// "01", "12": the number of a player as window titles and the crossfader show it
    inline std::string playerNumber(int number)
    {
        return (number < 10 ? "0" : "") + std::to_string(number);
    }
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_PLAYER_NUMBER_HPP
