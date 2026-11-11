// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "slider.hpp"

#include "look.hpp"

#include <algorithm>

namespace djw::player_ui::sdl
{
    namespace
    {
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
        const float slot   = std::clamp(area.height / 3, 8.0F, 14.0F);
        const float slot_y = area.y + ((area.height - slot) / 2);

        look::groove(canvas, {area.x, slot_y, area.width, slot}, false,
                     show_fill ? (knob_x - area.x) / std::max(1.0F, area.width) : -1.0F, accent);
        look::knob(canvas, {knob_x - (knob / 2), area.y, knob, area.height}, is_held, false);
    }
} // namespace djw::player_ui::sdl
