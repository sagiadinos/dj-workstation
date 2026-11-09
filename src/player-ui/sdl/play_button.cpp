// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "play_button.hpp"

#include <algorithm>
#include <utility>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr Color background{50, 50, 50};
        constexpr Color background_pressed{90, 90, 90};
        constexpr Color symbol_stopped{0, 200, 80};
        constexpr Color symbol_playing{230, 160, 0};
    } // namespace

    bool PlayButton::takePress() noexcept
    {
        return std::exchange(press_pending, false);
    }

    void PlayButton::pointerDown(Point)
    {
        pressed       = true;
        press_pending = true;
    }

    void PlayButton::pointerMove(Point) {}

    void PlayButton::pointerUp(Point)
    {
        pressed = false;
    }

    void PlayButton::draw(Canvas &canvas) const
    {
        const Rect &area = bounds();
        canvas.fillRect(area, pressed ? background_pressed : background);

        const float size = std::min(area.width, area.height) / 2;
        const float left = area.x + (area.width - size) / 2;
        const float top  = area.y + (area.height - size) / 2;
        if (showing_pause)
        {
            const float bar = size / 3;
            canvas.fillRect({left, top, bar, size}, symbol_playing);
            canvas.fillRect({left + size - bar, top, bar, size}, symbol_playing);
        }
        else
        {
            canvas.fillTriangle({left, top}, {left, top + size}, {left + size, top + size / 2}, symbol_stopped);
        }
    }
} // namespace djw::player_ui::sdl
