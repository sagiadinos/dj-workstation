// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "fader.hpp"

#include <algorithm>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr float knob_height = 24.0F;

        constexpr Color track_color{60, 60, 60};
        constexpr Color level_color{0, 150, 200};
        constexpr Color knob_color{220, 220, 220};
        constexpr Color knob_held_color{255, 255, 255};
    } // namespace

    void Fader::setValue(float value) noexcept
    {
        current = std::clamp(value, 0.0F, 1.0F);
    }

    void Fader::pointerDown(Point position)
    {
        held = true;
        follow(position);
    }

    void Fader::pointerMove(Point position)
    {
        follow(position);
    }

    void Fader::pointerUp(Point)
    {
        held = false;
    }

    Rect Fader::track() const noexcept
    {
        const Rect &area = bounds();
        const float knob = std::min(knob_height, area.height);
        return {area.x, area.y + knob / 2, area.width, area.height - knob};
    }

    void Fader::follow(Point position) noexcept
    {
        const Rect range = track();
        if (range.height <= 0.0F)
        {
            return;
        }
        setValue(1.0F - (position.y - range.y) / range.height);
    }

    void Fader::draw(Canvas &canvas) const
    {
        const Rect &area   = bounds();
        const Rect  range  = track();
        const float knob_y = range.y + (1.0F - current) * range.height;
        const float slot   = std::max(4.0F, area.width / 6);
        const float slot_x = area.x + (area.width - slot) / 2;
        const float knob   = std::min(knob_height, area.height);

        canvas.fillRect({slot_x, range.y, slot, range.height}, track_color);
        canvas.fillRect({slot_x, knob_y, slot, range.y + range.height - knob_y}, level_color);
        canvas.fillRect({area.x, knob_y - knob / 2, area.width, knob}, held ? knob_held_color : knob_color);
    }
} // namespace djw::player_ui::sdl
