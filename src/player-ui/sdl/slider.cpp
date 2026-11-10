// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "slider.hpp"

#include <algorithm>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr Color track_color{60, 60, 60};
        constexpr Color knob_color{220, 220, 220};
        constexpr Color knob_held_color{255, 255, 255};
    } // namespace

    void Slider::setValue(float value) noexcept
    {
        current = std::clamp(value, 0.0F, 1.0F);
    }

    void Slider::pointerDown(Point position)
    {
        is_held = true;
        follow(position);
    }

    void Slider::pointerMove(Point position)
    {
        follow(position);
    }

    void Slider::pointerUp(Point)
    {
        is_held = false;
    }

    Rect Slider::track() const noexcept
    {
        const Rect &area = bounds();
        const float knob = std::min(knob_width, area.width);
        return {area.x + (knob / 2), area.y, area.width - knob, area.height};
    }

    void Slider::follow(Point position) noexcept
    {
        const Rect range = track();
        if (range.width <= 0.0F)
        {
            return;
        }
        const float before = current;
        setValue((position.x - range.x) / range.width);
        changed = changed || current != before;
    }

    void Slider::draw(Canvas &canvas) const
    {
        const Rect &area   = bounds();
        const Rect  range  = track();
        const float knob   = std::min(knob_width, area.width);
        const float knob_x = range.x + (current * range.width);
        const float slot   = std::max(4.0F, area.height / 6);
        const float slot_y = area.y + ((area.height - slot) / 2);

        canvas.fillRect({range.x, slot_y, range.width, slot}, track_color);
        canvas.fillRect({range.x, slot_y, knob_x - range.x, slot}, accent);
        canvas.fillRect({knob_x - (knob / 2), area.y, knob, area.height}, is_held ? knob_held_color : knob_color);
    }
} // namespace djw::player_ui::sdl
