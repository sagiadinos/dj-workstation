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
        constexpr Color held_background{85, 85, 85};
        constexpr Color highlight_background{40, 75, 120};
        constexpr Color disabled_background{42, 42, 42};
        constexpr Color text_color{230, 230, 230};
        constexpr Color disabled_text_color{110, 110, 110};

        /// A custom color a little lighter, for a button that is held
        Color lighter(Color color)
        {
            const auto more = [](unsigned char value) { return static_cast<unsigned char>(std::min(255, value + 50)); };
            return {more(color.red), more(color.green), more(color.blue), color.alpha};
        }
    } // namespace

    void TextButton::setEnabled(bool on) noexcept
    {
        enabled = on;
    }

    void TextButton::pointerDown(Point /*position*/)
    {
        if (!enabled)
        {
            return;
        }
        down      = true;
        down_edge = true;
    }

    void TextButton::pointerUp(Point position)
    {
        pressed = down && bounds().contains(position);
        up_edge = up_edge || down;
        down    = false;
    }

    void TextButton::draw(Canvas &canvas) const
    {
        const Rect &area = bounds();
        if (area.width <= 0.0F || area.height <= 0.0F)
        {
            return;
        }
        Color fill = highlighted ? highlight_background : background;
        if (custom_background)
        {
            fill = down ? lighter(*custom_background) : *custom_background;
        }
        else if (down)
        {
            fill = held_background;
        }
        Color ink = custom_text.value_or(text_color);
        if (!enabled)
        {
            fill = disabled_background;
            ink  = disabled_text_color;
        }
        canvas.fillRect(area, fill);
        const float width = std::min(canvas.textWidth(text), area.width - (2 * padding));
        canvas.textElided({area.x + ((area.width - width) / 2), area.y + ((area.height - canvas.textHeight()) / 2)},
                          text, ink, area.width - (2 * padding));
    }
} // namespace djw::player_ui::sdl
