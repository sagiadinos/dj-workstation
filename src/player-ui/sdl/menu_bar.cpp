// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "menu_bar.hpp"
#include "look.hpp"

#include <algorithm>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr float padding = 14.0F;

        constexpr Color bar_background{32, 32, 32};
        constexpr Color open_background{40, 75, 120};
        constexpr Color dropdown_background{48, 48, 48};
        constexpr Color separator{80, 80, 80};
        constexpr Color text_color{230, 230, 230};
        constexpr Color dim_color{120, 120, 120};
    } // namespace

    void MenuBar::setMenus(std::vector<Menu> menus_)
    {
        menus = std::move(menus_);
        if (open && *open >= menus.size())
        {
            close();
        }
    }

    void MenuBar::place(const Rect &bar_area, const Rect &window_area)
    {
        bar    = bar_area;
        window = window_area;
        refreshBounds();
    }

    void MenuBar::refreshBounds()
    {
        setBounds(open ? window : bar);
    }

    void MenuBar::close()
    {
        open.reset();
        pressed = {};
        refreshBounds();
    }

    Rect MenuBar::titleArea(std::size_t menu) const
    {
        const float count = static_cast<float>(std::max<std::size_t>(menus.size(), 1));
        const float width = std::min(title_width, bar.width / count);
        const float step  = static_cast<float>(menu) * width;
        const float x     = rtl ? bar.x + bar.width - step - width : bar.x + step;
        return {x, bar.y, width, bar.height};
    }

    Rect MenuBar::dropdownArea(std::size_t menu) const
    {
        const Rect  title  = titleArea(menu);
        const float width  = std::min(dropdown_width, window.width);
        const float height = static_cast<float>(menus[menu].items.size()) * item_height;
        float       x      = rtl ? title.x + title.width - width : title.x;
        x                  = std::min(x, window.x + window.width - width);
        x                  = std::max(x, window.x);
        return {x, bar.y + bar.height, width, height};
    }

    Rect MenuBar::itemArea(std::size_t menu, std::size_t item) const
    {
        const Rect area = dropdownArea(menu);
        return {area.x, area.y + (static_cast<float>(item) * item_height), area.width, item_height};
    }

    MenuBar::Target MenuBar::hit(Point position) const
    {
        for (std::size_t menu = 0; menu < menus.size(); ++menu)
        {
            if (titleArea(menu).contains(position))
            {
                return {Target::Kind::Title, menu};
            }
        }
        if (!open)
        {
            return {};
        }
        for (std::size_t item = 0; item < menus[*open].items.size(); ++item)
        {
            if (itemArea(*open, item).contains(position))
            {
                return {Target::Kind::Item, item};
            }
        }
        return {Target::Kind::Outside, 0};
    }

    void MenuBar::pointerDown(Point position)
    {
        pressed = hit(position);
    }

    void MenuBar::pointerUp(Point position)
    {
        const Target target = hit(position);
        const Target down   = std::exchange(pressed, {});
        if (target != down)
        {
            return; // slid off: no press
        }
        switch (target.kind)
        {
            case Target::Kind::Title:
                if (open == target.index)
                {
                    close();
                }
                else
                {
                    open = target.index;
                    refreshBounds();
                }
                break;
            case Target::Kind::Item:
                if (open && menus[*open].items[target.index].enabled)
                {
                    chosen = menus[*open].items[target.index].id;
                    close();
                }
                break;
            case Target::Kind::Outside:
                close();
                break;
            case Target::Kind::None:
                break;
        }
    }

    void MenuBar::draw(Canvas &canvas) const
    {
        if (bar.width <= 0.0F || bar.height <= 0.0F)
        {
            return;
        }
        canvas.fillRoundedRect(bar, 0.0F, {58, 60, 66}, {30, 31, 35});
        canvas.fillRect({bar.x, bar.y + bar.height - 1.0F, bar.width, 1.0F}, separator);
        const float text_y = (bar.height - canvas.textHeight()) / 2;
        for (std::size_t menu = 0; menu < menus.size(); ++menu)
        {
            const Rect title = titleArea(menu);
            if (open == menu)
            {
                canvas.fillRoundedRect(title, 0.0F, {62, 112, 172}, {36, 72, 118});
            }
            const float width = std::min(canvas.textWidth(menus[menu].title), title.width - (2 * padding));
            canvas.textElided({title.x + ((title.width - width) / 2), bar.y + text_y}, menus[menu].title, text_color,
                              title.width - (2 * padding));
        }
        if (!open)
        {
            return;
        }

        const Menu &shown = menus[*open];
        const Rect  box   = dropdownArea(*open);
        canvas.fillRoundedRect(box.inset(-1.0F), 4.0F, look::outline, look::outline);
        canvas.fillRoundedRect(box, 3.0F, {64, 66, 72}, {44, 46, 51});
        for (std::size_t item = 0; item < shown.items.size(); ++item)
        {
            const MenuItem &entry = shown.items[item];
            const Rect      area  = itemArea(*open, item);
            if (entry.checked)
            {
                canvas.fillRoundedRect(area, 0.0F, {62, 112, 172}, {36, 72, 118});
            }
            const float text_y    = area.y + ((area.height - canvas.textHeight()) / 2);
            float       available = area.width - (2 * padding);
            if (!entry.shortcut.empty())
            {
                // At the end of the entry: right in left-to-right layout, left in right-to-left
                const float shortcut_width = canvas.textWidth(entry.shortcut);
                const float shortcut_x = rtl ? area.x + padding : area.x + area.width - padding - shortcut_width;
                canvas.text({shortcut_x, text_y}, entry.shortcut, dim_color);
                available = std::max(0.0F, available - shortcut_width - padding);
            }
            const float width = std::min(canvas.textWidth(entry.label), available);
            const float x     = rtl ? area.x + area.width - padding - width : area.x + padding;
            canvas.textElided({x, text_y}, entry.label, entry.enabled ? text_color : dim_color, available);
        }
        canvas.fillRect({box.x, box.y + box.height, box.width, 1.0F}, separator);
    }
} // namespace djw::player_ui::sdl
