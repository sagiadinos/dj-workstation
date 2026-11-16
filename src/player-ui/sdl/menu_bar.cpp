// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "menu_bar.hpp"
#include "look.hpp"

#include <algorithm>
#include <numeric>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr float padding = 14.0F;
        /// The check mark of an entry that is on, in a column of its own at the start of every entry
        constexpr float check_size   = 14.0F;
        constexpr float check_column = check_size + 10.0F;

    } // namespace

    void MenuBar::setMenus(std::vector<Menu> new_menus)
    {
        menus = std::move(new_menus);
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

    std::vector<float> MenuBar::titleWidths() const
    {
        std::vector<float> widths;
        widths.reserve(menus.size());
        float total = 0.0F;
        for (std::size_t menu = 0; menu < menus.size(); ++menu)
        {
            const float text = menu < measured.size() ? measured[menu] : 0.0F;
            widths.push_back(std::max(min_title_width, text + (2 * padding)));
            total += widths.back();
        }
        if (total <= bar.width || widths.empty())
        {
            return widths;
        }
        // Too wide together: the widest titles are cut to one width, so the short ones stay whole
        std::vector<float> sorted = widths;
        std::ranges::sort(sorted);
        float       room = std::max(0.0F, bar.width);
        float       cap  = 0.0F;
        std::size_t left = sorted.size();
        for (const float width : sorted)
        {
            cap = room / static_cast<float>(left);
            if (width > cap)
            {
                break;
            }
            room -= width;
            --left;
        }
        std::ranges::for_each(widths, [cap](float &width) { width = std::min(width, cap); });
        return widths;
    }

    Rect MenuBar::titleArea(std::size_t menu) const
    {
        const std::vector<float> widths = titleWidths();
        if (menu >= widths.size())
        {
            return {};
        }
        float step = 0.0F;
        for (std::size_t before = 0; before < menu; ++before)
        {
            step += widths[before];
        }
        const float width = widths[menu];
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
        canvas.fillRoundedRect(bar, 0.0F, look::colors().menu_bar_top, look::colors().menu_bar_bottom);
        canvas.fillRect({bar.x, bar.y + bar.height - 1.0F, bar.width, 1.0F}, look::colors().separator);
        const float text_y = (bar.height - canvas.textHeight()) / 2;
        measured.clear();
        for (const Menu &menu : menus)
        {
            measured.push_back(canvas.textWidth(menu.title));
        }
        for (std::size_t menu = 0; menu < menus.size(); ++menu)
        {
            const Rect title = titleArea(menu);
            if (open == menu)
            {
                canvas.fillRoundedRect(title, 0.0F, look::colors().highlight_top, look::colors().highlight_bottom);
            }
            const float width = std::min(canvas.textWidth(menus[menu].title), title.width - (2 * padding));
            canvas.textElided({title.x + ((title.width - width) / 2), bar.y + text_y}, menus[menu].title,
                              open == menu ? look::colors().highlight_text : look::colors().text,
                              title.width - (2 * padding));
        }
        if (!note.empty())
        {
            // At the end of the bar, if the titles leave room; under an open drop-down
            const std::vector<float> widths     = titleWidths();
            const float              titles     = std::accumulate(widths.begin(), widths.end(), 0.0F);
            const float              note_width = canvas.textWidth(note);
            if (titles + note_width + (2 * padding) <= bar.width)
            {
                const float x = rtl ? bar.x + padding : bar.x + bar.width - padding - note_width;
                canvas.text({x, bar.y + text_y}, note, look::colors().text_dim);
            }
        }
        if (!open)
        {
            return;
        }

        const Menu &shown = menus[*open];
        const Rect  box   = dropdownArea(*open);
        canvas.fillRoundedRect(box.inset(-1.0F), 4.0F, look::colors().outline, look::colors().outline);
        canvas.fillRoundedRect(box, 3.0F, look::colors().menu_box_top, look::colors().menu_box_bottom);
        for (std::size_t item = 0; item < shown.items.size(); ++item)
        {
            const MenuItem &entry = shown.items[item];
            const Rect      area  = itemArea(*open, item);
            const Color     ink   = entry.enabled ? look::colors().text : look::colors().text_dim;
            if (entry.separated && item > 0)
            {
                canvas.fillRect({area.x + padding, area.y, std::max(0.0F, area.width - (2 * padding)), 1.0F},
                                look::colors().separator);
            }
            if (entry.checked)
            {
                // At the start of the entry: left in left-to-right layout, right in right-to-left
                const float mark_x = rtl ? area.x + area.width - padding - check_size : area.x + padding;
                look::checkMark(canvas, {mark_x, area.y + ((area.height - check_size) / 2), check_size, check_size},
                                ink);
            }
            const float text_y    = area.y + ((area.height - canvas.textHeight()) / 2);
            float       available = std::max(0.0F, area.width - (2 * padding) - check_column);
            if (!entry.shortcut.empty())
            {
                // At the end of the entry: right in left-to-right layout, left in right-to-left
                const float shortcut_width = canvas.textWidth(entry.shortcut);
                const float shortcut_x     = rtl ? area.x + padding : area.x + area.width - padding - shortcut_width;
                canvas.text({shortcut_x, text_y}, entry.shortcut, look::colors().text_dim);
                available = std::max(0.0F, available - shortcut_width - padding);
            }
            const float width = std::min(canvas.textWidth(entry.label), available);
            const float x =
                rtl ? area.x + area.width - padding - check_column - width : area.x + padding + check_column;
            canvas.textElided({x, text_y}, entry.label, ink, available);
        }
        canvas.fillRect({box.x, box.y + box.height, box.width, 1.0F}, look::colors().separator);
    }
} // namespace djw::player_ui::sdl
