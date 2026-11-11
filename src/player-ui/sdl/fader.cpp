// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "fader.hpp"

#include "look.hpp"

#include <algorithm>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr float knob_height = 24.0F;

    } // namespace

    void Fader::setValue(float value) noexcept
    {
        current = std::clamp(value, 0.0F, 1.0F);
    }

    void Fader::pointerDown(Point position)
    {
        const auto now = clock();
        if (last_down && now - *last_down <= double_tap_time)
        {
            double_tapped = true;
            last_down.reset();
        }
        else
        {
            last_down = now;
        }
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
        const float slot   = std::clamp(area.width / 4, 10.0F, 18.0F);
        const float slot_x = area.x + (area.width - slot) / 2;
        const float knob   = std::min(knob_height, area.height);

        // The groove spans the full travel and a little more, so the grip never leaves it
        const Rect groove{slot_x, area.y, slot, area.height};
        look::groove(canvas, groove, true, (range.y + range.height - knob_y + (knob / 2)) / std::max(1.0F, area.height));
        look::knob(canvas, {area.x, knob_y - knob / 2, area.width, knob}, held, true);
    }
} // namespace djw::player_ui::sdl
