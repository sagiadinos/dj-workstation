// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "look.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace djw::player_ui::sdl::look
{
    namespace
    {
        constexpr Color metal_top{40, 42, 46};
        constexpr Color metal_bottom{24, 25, 28};
        constexpr Color frame{70, 72, 78};
    } // namespace

    Color shade(Color color, int amount) noexcept
    {
        const auto move = [amount](unsigned char value)
        { return static_cast<unsigned char>(std::clamp(static_cast<int>(value) + amount, 0, 255)); };
        return {move(color.red), move(color.green), move(color.blue), color.alpha};
    }

    void background(Canvas &canvas, const Rect &area)
    {
        canvas.fillRoundedRect(area, 0.0F, metal_top, metal_bottom);
        // Brushed: fine horizontal lines, a little lighter or darker, the same every frame
        constexpr float line_step = 3.0F;
        std::uint32_t   seed      = 12'345;
        const auto      lines     = static_cast<int>(std::ceil(area.height / line_step));
        for (int line_index = 0; line_index < lines; ++line_index)
        {
            const float y        = area.y + (static_cast<float>(line_index) * line_step);
            seed                 = (seed * 1'103'515'245U) + 12'345U;
            const auto  strength = static_cast<unsigned char>((seed >> 16U) % 9U);
            const bool  light    = ((seed >> 24U) & 1U) != 0U;
            const Color line     = light ? Color{255, 255, 255, strength} : Color{0, 0, 0, strength};
            canvas.fillRect({area.x, y, area.width, 1.0F}, line);
        }
    }

    void raised(Canvas &canvas, const Rect &area, Color base, bool pressed, float radius)
    {
        if (area.width <= 0.0F || area.height <= 0.0F)
        {
            return;
        }
        canvas.fillRoundedRect(area, radius, outline, outline);
        const Rect  body  = area.inset(1.0F);
        const Color light = shade(base, pressed ? -10 : 28);
        const Color dark  = shade(base, pressed ? -32 : -18);
        canvas.fillRoundedRect(body, std::max(0.0F, radius - 1.0F), pressed ? dark : light, pressed ? light : dark);
        if (!pressed && body.width > 2 * radius)
        {
            // The glossy edge along the top
            canvas.fillRect({body.x + radius, body.y + 1.0F, body.width - (2 * radius), 1.0F}, {255, 255, 255, 60});
        }
    }

    void sunken(Canvas &canvas, const Rect &area, Color fill, float radius)
    {
        if (area.width <= 0.0F || area.height <= 0.0F)
        {
            return;
        }
        canvas.fillRoundedRect(area, radius, frame, frame);
        canvas.fillRoundedRect(area.inset(1.0F), std::max(0.0F, radius - 1.0F), fill, fill);
    }

    void groove(Canvas &canvas, const Rect &area, bool vertical, float level, Color fill)
    {
        if (area.width <= 0.0F || area.height <= 0.0F)
        {
            return;
        }
        const float radius = std::min(area.width, area.height) / 2;
        canvas.fillRoundedRect(area, radius, outline, outline);
        const Rect inner = area.inset(1.0F);
        canvas.fillRoundedRect(inner, std::max(0.0F, radius - 1.0F), {18, 19, 22}, {44, 46, 51});
        if (level < 0.0F)
        {
            return;
        }
        const Rect  lit     = inner.inset(1.0F);
        const float clamped = std::clamp(level, 0.0F, 1.0F);
        if (vertical)
        {
            const float height = lit.height * clamped;
            canvas.fillRoundedRect({lit.x, lit.y + lit.height - height, lit.width, height},
                                   std::min(lit.width, height) / 2, shade(fill, 40), shade(fill, -40));
        }
        else
        {
            canvas.fillRoundedRect({lit.x, lit.y, lit.width * clamped, lit.height},
                                   std::min(lit.height, lit.width * clamped) / 2, shade(fill, 40), shade(fill, -40));
        }
    }

    void knob(Canvas &canvas, const Rect &area, bool held, bool mark_horizontal)
    {
        if (area.width <= 0.0F || area.height <= 0.0F)
        {
            return;
        }
        canvas.fillRoundedRect(area, 4.0F, outline, outline);
        const Rect body = area.inset(1.0F);
        canvas.fillRoundedRect(body, 3.0F, held ? Color{250, 250, 252} : Color{228, 229, 233},
                               held ? Color{150, 152, 158} : Color{116, 118, 124});
        const Color mark{40, 42, 46, 200};
        if (mark_horizontal)
        {
            canvas.fillRect({body.x + 3.0F, body.y + (body.height / 2) - 0.5F, std::max(0.0F, body.width - 6.0F), 1.0F},
                            mark);
        }
        else
        {
            canvas.fillRect({body.x + (body.width / 2) - 0.5F, body.y + 3.0F, 1.0F, std::max(0.0F, body.height - 6.0F)},
                            mark);
        }
    }

    void folderIcon(Canvas &canvas, const Rect &area, Color color)
    {
        if (area.width <= 0.0F || area.height <= 0.0F)
        {
            return;
        }
        // A folder seen from the front: the tab and the back wall darker, the front flap in the color
        const Color back = shade(color, -70);
        canvas.fillRoundedRect({area.x, area.y + (area.height * 0.10F), area.width * 0.42F, area.height * 0.30F}, 2.0F,
                               back, back);
        canvas.fillRoundedRect({area.x, area.y + (area.height * 0.24F), area.width, area.height * 0.66F}, 3.0F, back,
                               back);
        canvas.fillRoundedRect({area.x, area.y + (area.height * 0.42F), area.width * 0.94F, area.height * 0.48F}, 3.0F,
                               color, shade(color, -30));
    }
} // namespace djw::player_ui::sdl::look
