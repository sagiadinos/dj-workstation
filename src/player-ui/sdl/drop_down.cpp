// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "drop_down.hpp"
#include "look.hpp"

#include <algorithm>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr float padding   = 10.0F;
        constexpr float arrow_box = 10.0F;

        constexpr Color text_color{230, 230, 230};
    } // namespace

    void DropDown::setItems(std::vector<std::string> new_items)
    {
        items = std::move(new_items);
        if (selected && *selected >= items.size())
        {
            selected.reset();
        }
        if (open && items.empty())
        {
            close();
        }
    }

    void DropDown::place(const Rect &closed_area, const Rect &window_area)
    {
        box    = closed_area;
        window = window_area;
        refreshBounds();
    }

    void DropDown::refreshBounds()
    {
        setBounds(open ? window : box);
    }

    void DropDown::close()
    {
        open    = false;
        pressed = {};
        refreshBounds();
    }

    Rect DropDown::listArea() const
    {
        // Below the box; above it if there is no room below
        const float height = static_cast<float>(items.size()) * item_height;
        float       y      = box.y + box.height;
        if (y + height > window.y + window.height && box.y - height >= window.y)
        {
            y = box.y - height;
        }
        return {box.x, y, box.width, height};
    }

    Rect DropDown::itemArea(std::size_t index) const
    {
        const Rect list = listArea();
        return {list.x, list.y + (static_cast<float>(index) * item_height), list.width, item_height};
    }

    DropDown::Target DropDown::hit(Point position) const
    {
        if (box.contains(position))
        {
            return {Target::Kind::Box, 0};
        }
        if (!open)
        {
            return {};
        }
        for (std::size_t index = 0; index < items.size(); ++index)
        {
            if (itemArea(index).contains(position))
            {
                return {Target::Kind::Item, index};
            }
        }
        return {Target::Kind::Outside, 0};
    }

    void DropDown::pointerDown(Point position)
    {
        pressed = hit(position);
    }

    void DropDown::pointerUp(Point position)
    {
        const Target target = hit(position);
        const Target down   = std::exchange(pressed, {});
        if (target != down)
        {
            return;
        }
        switch (target.kind)
        {
            case Target::Kind::Box:
                if (open)
                {
                    close();
                }
                else if (!items.empty())
                {
                    open = true;
                    refreshBounds();
                }
                break;
            case Target::Kind::Item:
                chosen = target.index;
                close();
                break;
            case Target::Kind::Outside:
                close();
                break;
            case Target::Kind::None:
                break;
        }
    }

    void DropDown::draw(Canvas &canvas) const
    {
        if (box.width <= 0.0F || box.height <= 0.0F)
        {
            return;
        }
        look::raised(canvas, box, look::button_gray, false, 6.0F);
        const float text_y = box.y + ((box.height - canvas.textHeight()) / 2);
        const float room   = box.width - (3 * padding) - arrow_box;
        if (selected && *selected < items.size())
        {
            canvas.textElided({box.x + padding, text_y}, items[*selected], text_color, room);
        }
        // The arrow at the right edge
        const float arrow_x = box.x + box.width - padding - arrow_box;
        const float arrow_y = box.y + ((box.height - (arrow_box / 2)) / 2);
        canvas.fillTriangle({arrow_x, arrow_y}, {arrow_x + arrow_box, arrow_y},
                            {arrow_x + (arrow_box / 2), arrow_y + (arrow_box / 2)}, text_color);
        if (!open)
        {
            return;
        }
        canvas.fillRoundedRect(listArea(), 4.0F, look::outline, look::outline);
        for (std::size_t index = 0; index < items.size(); ++index)
        {
            const Rect area = itemArea(index);
            if (selected == index)
            {
                canvas.fillRoundedRect(area.inset(1.0F), 0.0F, {62, 112, 172}, {36, 72, 118});
            }
            else
            {
                canvas.fillRoundedRect(area.inset(1.0F), 0.0F, {64, 66, 72}, {50, 52, 57});
            }
            canvas.textElided({area.x + padding, area.y + ((area.height - canvas.textHeight()) / 2)}, items[index],
                              text_color, area.width - (2 * padding));
        }
    }
} // namespace djw::player_ui::sdl
