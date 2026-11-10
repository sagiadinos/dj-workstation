// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_CLOCK_HPP
#define DJW_PLAYER_UI_SDL_CLOCK_HPP

#include <chrono>
#include <functional>

namespace djw::player_ui::sdl
{
    /// Source of the current time for the UI; tests pass their own to control timing
    using Clock = std::function<std::chrono::steady_clock::time_point()>;

    inline Clock steadyClock()
    {
        return [] { return std::chrono::steady_clock::now(); };
    }
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_CLOCK_HPP
