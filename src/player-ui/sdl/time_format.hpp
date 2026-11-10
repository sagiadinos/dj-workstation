// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_TIME_FORMAT_HPP
#define DJW_PLAYER_UI_SDL_TIME_FORMAT_HPP

#include <string>

namespace djw::player_ui::sdl
{
    /// Track position as "m:ss.t" (minutes, seconds, tenths), e.g. "3:07.4". Negative values show as 0.
    std::string formatPosition(double seconds);

    /// Track length as "m:ss", rounded to whole seconds, e.g. "4:05". Negative values show as 0.
    std::string formatDuration(double seconds);
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_TIME_FORMAT_HPP
