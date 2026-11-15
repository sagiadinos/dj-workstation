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
        Color fill = highlighted ? look::colors().button_blue : look::colors().button_gray;
        if (custom_background)
        {
            fill = *custom_background;
        }
        Color ink = custom_text.value_or(look::colors().text);
        if (!enabled)
        {
            fill = look::colors().disabled_background;
            ink  = look::colors().disabled_text;
        }
        look::raised(canvas, area, fill, down && enabled);

        // An LED takes the top third, icon and label share the rest
        Rect content = area;
        if (led)
        {
            const float led_height = area.height * 0.32F;
            look::led(canvas, {area.x, area.y + (area.height * 0.06F), area.width, led_height}, *led);
            content.y      = area.y + (area.height * 0.06F) + led_height;
            content.height = area.y + area.height - content.y;
        }

        // Icon and label as one group, in the middle
        const float icon_size =
            icon == Icon::None ? 0.0F
                               : std::max(0.0F, std::min(led ? content.height * 0.75F : area.height - 16.0F, 26.0F));
        const float icon_gap = icon == Icon::None || text.empty() ? 0.0F : 10.0F;
        const float room     = area.width - (2 * padding) - icon_size - icon_gap;
        const float width    = std::min(canvas.textWidth(text), std::max(0.0F, room));
        const float group    = icon_size + icon_gap + width;
        const float left     = area.x + ((area.width - group) / 2);
        const Rect  icon_area{left, content.y + ((content.height - icon_size) / 2), icon_size, icon_size};
        if (icon == Icon::ClearFilter && icon_size > 0.0F)
        {
            look::clearFilterIcon(canvas, icon_area, ink);
        }
        if ((icon == Icon::LanesHorizontal || icon == Icon::LanesVertical) && icon_size > 0.0F)
        {
            look::lanesIcon(canvas, icon_area, icon == Icon::LanesVertical, ink);
        }
        if (icon == Icon::Headphones && icon_size > 0.0F)
        {
            look::headphonesIcon(canvas, icon_area, ink);
        }
        canvas.textElided({left + icon_size + icon_gap, content.y + ((content.height - canvas.textHeight()) / 2)}, text,
                          ink, std::max(0.0F, room));
    }
} // namespace djw::player_ui::sdl
