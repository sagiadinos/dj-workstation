// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "text_field.hpp"

#include "look.hpp"

#include "utf8.hpp"

#include <algorithm>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr float padding      = 10.0F;
        constexpr float cursor_width = 2.0F;

        constexpr Color focus_color{80, 160, 255};
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
        else if (const auto *key = std::get_if<EditKey>(&input))
        {
            switch (*key)
            {
                case EditKey::Backspace:
                    popCodePoint(content);
                    break;
                case EditKey::Escape:
                    content.clear();
                    break;
                case EditKey::Enter:
                case EditKey::Tab:
                case EditKey::BackTab:
                case EditKey::Up:
                case EditKey::Down:
                case EditKey::PageUp:
                case EditKey::PageDown:
                case EditKey::Left:
                case EditKey::Right:
                case EditKey::ShiftLeft:
                case EditKey::ShiftRight:
                case EditKey::ShiftUp:
                case EditKey::ShiftDown:
                case EditKey::CtrlLeft:
                case EditKey::CtrlRight:
                case EditKey::CtrlUp:
                case EditKey::CtrlDown:
                case EditKey::CtrlShiftLeft:
                case EditKey::CtrlShiftRight:
                case EditKey::CtrlShiftUp:
                case EditKey::CtrlShiftDown:
                    break; // the view moves between fields and rows
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
        canvas.fillRoundedRect(area, 5.0F, has_focus ? focus_color : look::colors().field_frame,
                               has_focus ? focus_color : look::colors().field_frame);
        const Rect inner = area.inset(2.0F);
        canvas.fillRoundedRect(inner, 4.0F, look::colors().field_top, look::colors().field_bottom);

        const float text_y    = area.y + ((area.height - canvas.textHeight()) / 2);
        const float max_width = std::max(0.0F, inner.width - (2 * padding) - cursor_width);
        if (content.empty())
        {
            canvas.textElided({inner.x + padding, text_y}, placeholder, look::colors().text_dim, max_width);
        }
        else
        {
            canvas.textElided({inner.x + padding, text_y}, content, look::colors().text, max_width);
        }
        if (has_focus)
        {
            const float cursor_x = inner.x + padding + std::min(canvas.textWidth(content), max_width);
            canvas.fillRect({cursor_x, text_y, cursor_width, canvas.textHeight()}, look::colors().text);
        }
    }
} // namespace djw::player_ui::sdl
