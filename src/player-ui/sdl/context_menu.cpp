// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "context_menu.hpp"
#include "look.hpp"

#include <algorithm>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr float padding = 12.0F;

    } // namespace

    void ContextMenu::setItems(std::vector<MenuItem> new_items)
    {
        items = std::move(new_items);
        if (items.empty())
        {
            close();
        }
    }

    void ContextMenu::place(const Rect &window_area)
    {
        window = window_area;
        setBounds(opened ? window : Rect{});
    }

    void ContextMenu::open(Point position)
    {
        if (items.empty())
        {
            return;
        }
        at      = position;
        opened  = true;
        pressed = none;
        setBounds(window);
    }

    void ContextMenu::close()
    {
        opened  = false;
        pressed = none;
        setBounds({});
    }

    Rect ContextMenu::menuArea() const
    {
        const float height = static_cast<float>(items.size()) * item_height;
        const float width  = std::min(menu_width, window.width);
        // At the point, moved inside the window
        const float x = std::clamp(at.x, window.x, std::max(window.x, window.x + window.width - width));
        const float y = std::clamp(at.y, window.y, std::max(window.y, window.y + window.height - height));
        return {x, y, width, height};
    }

    Rect ContextMenu::itemArea(std::size_t index) const
    {
        const Rect menu = menuArea();
        return {menu.x, menu.y + (static_cast<float>(index) * item_height), menu.width, item_height};
    }

    std::size_t ContextMenu::hit(Point position) const
    {
        if (!opened)
        {
            return none;
        }
        for (std::size_t index = 0; index < items.size(); ++index)
        {
            if (itemArea(index).contains(position))
            {
                return index;
            }
        }
        return outside;
    }

    void ContextMenu::pointerDown(Point position)
    {
        pressed = hit(position);
    }

    void ContextMenu::pointerUp(Point position)
    {
        const std::size_t target = hit(position);
        const std::size_t down   = std::exchange(pressed, none);
        if (target != down || target == none)
        {
            return;
        }
        if (target == outside)
        {
            close();
        }
        else if (items[target].enabled)
        {
            chosen = items[target].id;
            close();
        }
    }

    void ContextMenu::draw(Canvas &canvas) const
    {
        if (!opened)
        {
            return;
        }
        canvas.fillRoundedRect(menuArea(), 4.0F, look::colors().outline, look::colors().outline);
        for (std::size_t index = 0; index < items.size(); ++index)
        {
            const Rect area = itemArea(index);
            if (pressed == index)
            {
                canvas.fillRoundedRect(area.inset(1.0F), 0.0F, look::colors().highlight_top,
                                       look::colors().highlight_bottom);
            }
            else
            {
                canvas.fillRoundedRect(area.inset(1.0F), 0.0F, look::colors().item_top, look::colors().item_bottom);
            }
            canvas.textElided({area.x + padding, area.y + ((area.height - canvas.textHeight()) / 2)},
                              items[index].label,
                              !items[index].enabled ? look::colors().text_dim
                              : pressed == index    ? look::colors().highlight_text
                                                    : look::colors().text,
                              area.width - (2 * padding));
        }
    }
} // namespace djw::player_ui::sdl
