// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "check_box.hpp"

#include "look.hpp"

#include <algorithm>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr float box_size = 22.0F;
        constexpr float gap      = 8.0F;

        constexpr Color box_color{35, 35, 35};
        constexpr Color frame_color{150, 150, 150};
        constexpr Color check_color{0, 190, 90};
        constexpr Color text_color{230, 230, 230};
    } // namespace

    void CheckBox::pointerUp(Point position)
    {
        if (down && bounds().contains(position))
        {
            on      = !on;
            toggled = true;
        }
        down = false;
    }

    void CheckBox::draw(Canvas &canvas) const
    {
        const Rect &area = bounds();
        if (area.width <= 0.0F || area.height <= 0.0F)
        {
            return;
        }
        const float size = std::min(box_size, area.height);
        const Rect  box{area.x, area.y + ((area.height - size) / 2), size, size};
        look::sunken(canvas, box, box_color, 5.0F);
        if (on)
        {
            canvas.fillRoundedRect(box.inset(5.0F), 3.0F, check_color, {0, 150, 70});
        }
        const float text_x = area.x + size + gap;
        canvas.textElided({text_x, area.y + ((area.height - canvas.textHeight()) / 2)}, text, text_color,
                          std::max(0.0F, area.x + area.width - text_x));
    }
} // namespace djw::player_ui::sdl
