// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "text_field.hpp"

#include "utf8.hpp"

#include <algorithm>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr float padding      = 10.0F;
        constexpr float cursor_width = 2.0F;

        constexpr Color frame_color{70, 70, 70};
        constexpr Color focus_color{80, 160, 255};
        constexpr Color text_color{230, 230, 230};
        constexpr Color placeholder_color{130, 130, 130};
    } // namespace

    bool TextField::input(const KeyboardInput &input)
    {
        if (!has_focus)
        {
            return false;
        }
        const std::string before = content;
        if (const auto *typed = std::get_if<std::string>(&input))
        {
            content += *typed;
        }
        else
        {
            switch (std::get<EditKey>(input))
            {
                case EditKey::Backspace:
                    popCodePoint(content);
                    break;
                case EditKey::Escape:
                    content.clear();
                    break;
                case EditKey::Enter:
                    break;
            }
        }
        return content != before;
    }

    void TextField::pointerDown(Point /*position*/)
    {
        has_focus       = true;
        focus_requested = true;
    }

    void TextField::draw(Canvas &canvas) const
    {
        const Rect &area = bounds();
        canvas.fillRoundedRect(area, 5.0F, has_focus ? focus_color : frame_color,
                               has_focus ? focus_color : frame_color);
        const Rect inner = area.inset(2.0F);
        canvas.fillRoundedRect(inner, 4.0F, {16, 17, 20}, {30, 32, 36});

        const float text_y    = area.y + ((area.height - canvas.textHeight()) / 2);
        const float max_width = std::max(0.0F, inner.width - (2 * padding) - cursor_width);
        if (content.empty())
        {
            canvas.textElided({inner.x + padding, text_y}, placeholder, placeholder_color, max_width);
        }
        else
        {
            canvas.textElided({inner.x + padding, text_y}, content, text_color, max_width);
        }
        if (has_focus)
        {
            const float cursor_x = inner.x + padding + std::min(canvas.textWidth(content), max_width);
            canvas.fillRect({cursor_x, text_y, cursor_width, canvas.textHeight()}, text_color);
        }
    }
} // namespace djw::player_ui::sdl
