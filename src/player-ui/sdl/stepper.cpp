// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "stepper.hpp"
#include "look.hpp"

#include <algorithm>
#include <string>

namespace djw::player_ui::sdl
{
    namespace
    {
    } // namespace

    void Stepper::setValue(int value) noexcept
    {
        current = std::clamp(value, low, high);
    }

    Stepper::Part Stepper::partAt(Point position) const noexcept
    {
        const Rect &area = bounds();
        if (!area.contains(position))
        {
            return Part::None;
        }
        const float side = std::min(area.height, area.width / 3);
        if (position.x < area.x + side)
        {
            return Part::Minus;
        }
        return position.x >= area.x + area.width - side ? Part::Plus : Part::None;
    }

    void Stepper::pointerDown(Point position)
    {
        pressed = partAt(position);
    }

    void Stepper::pointerUp(Point position)
    {
        const Part part = std::exchange(pressed, Part::None);
        if (part == Part::None || partAt(position) != part)
        {
            return;
        }
        const int before = current;
        setValue(current + (part == Part::Plus ? 1 : -1));
        changed = changed || current != before;
    }

    void Stepper::draw(Canvas &canvas) const
    {
        const Rect &area = bounds();
        if (area.width <= 0.0F || area.height <= 0.0F)
        {
            return;
        }
        const float side = std::min(area.height, area.width / 3);
        const Rect  minus{area.x, area.y, side, area.height};
        const Rect  plus{area.x + area.width - side, area.y, side, area.height};
        look::sunken(canvas, area, look::colors().field, 5.0F);
        look::raised(canvas, minus, look::colors().button_gray, pressed == Part::Minus, 5.0F);
        look::raised(canvas, plus, look::colors().button_gray, pressed == Part::Plus, 5.0F);

        const auto center = [&canvas](const Rect &where, const std::string &text, Color color)
        {
            canvas.text({where.x + ((where.width - canvas.textWidth(text)) / 2),
                         where.y + ((where.height - canvas.textHeight()) / 2)},
                        text, color);
        };
        center(minus, "-", current > low ? look::colors().text : look::colors().disabled_text);
        center(plus, "+", current < high ? look::colors().text : look::colors().disabled_text);
        center({area.x + side, area.y, area.width - (2 * side), area.height}, std::to_string(current),
               look::colors().text);
    }
} // namespace djw::player_ui::sdl
