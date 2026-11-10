// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "text_button.hpp"

#include <algorithm>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr float padding = 8.0F;

        constexpr Color background{55, 55, 55};
        constexpr Color highlight_background{40, 75, 120};
        constexpr Color text_color{230, 230, 230};
    } // namespace

    void TextButton::pointerUp(Point position)
    {
        pressed = down && bounds().contains(position);
        down    = false;
    }

    void TextButton::draw(Canvas &canvas) const
    {
        const Rect &area = bounds();
        if (area.width <= 0.0F || area.height <= 0.0F)
        {
            return;
        }
        canvas.fillRect(area, highlighted ? highlight_background : background);
        const float width = std::min(canvas.textWidth(text), area.width - (2 * padding));
        canvas.textElided({area.x + ((area.width - width) / 2), area.y + ((area.height - canvas.textHeight()) / 2)},
                          text, text_color, area.width - (2 * padding));
    }
} // namespace djw::player_ui::sdl
