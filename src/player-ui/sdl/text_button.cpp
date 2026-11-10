// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "text_button.hpp"

#include "look.hpp"

#include <algorithm>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr float padding = 8.0F;

        constexpr Color disabled_background{42, 42, 42};
        constexpr Color disabled_text_color{110, 110, 110};
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
        Color fill = highlighted ? look::button_blue : look::button_gray;
        if (custom_background)
        {
            fill = *custom_background;
        }
        Color ink = custom_text.value_or(look::text);
        if (!enabled)
        {
            fill = disabled_background;
            ink  = disabled_text_color;
        }
        look::raised(canvas, area, fill, down && enabled);

        // Icon and label as one group, in the middle
        const float icon_size = icon == Icon::None ? 0.0F : std::min(area.height - 16.0F, 26.0F);
        const float icon_gap  = icon == Icon::None ? 0.0F : 10.0F;
        const float room      = area.width - (2 * padding) - icon_size - icon_gap;
        const float width     = std::min(canvas.textWidth(text), std::max(0.0F, room));
        const float group     = icon_size + icon_gap + width;
        const float left      = area.x + ((area.width - group) / 2);
        if (icon == Icon::Folder && icon_size > 0.0F)
        {
            look::folderIcon(canvas, {left, area.y + ((area.height - icon_size) / 2), icon_size, icon_size}, ink);
        }
        canvas.textElided({left + icon_size + icon_gap, area.y + ((area.height - canvas.textHeight()) / 2)}, text, ink,
                          std::max(0.0F, room));
    }
} // namespace djw::player_ui::sdl
