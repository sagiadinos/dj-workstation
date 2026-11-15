// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "look.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <cstdint>

namespace djw::player_ui::sdl::look
{
    namespace
    {
        constexpr Palette dark{.window              = {20, 20, 20},
                               .metal_top           = {40, 42, 46},
                               .metal_bottom        = {24, 25, 28},
                               .frame               = {70, 72, 78},
                               .outline             = {14, 14, 16},
                               .text                = {232, 232, 235},
                               .text_dim            = {150, 152, 158},
                               .label               = {205, 205, 205},
                               .status              = {160, 160, 160},
                               .button_gray         = {68, 70, 75},
                               .button_blue         = {40, 75, 120},
                               .disabled_background = {42, 42, 42},
                               .disabled_text       = {110, 110, 110},
                               .field               = {35, 35, 35},
                               .field_top           = {16, 17, 20},
                               .field_bottom        = {30, 32, 36},
                               .field_frame         = {70, 70, 70},
                               .groove_top          = {18, 19, 22},
                               .groove_bottom       = {44, 46, 51},
                               .row                 = {28, 28, 28},
                               .row_alternate       = {34, 34, 34},
                               .secondary_text      = {160, 160, 160},
                               .missing_text        = {205, 105, 105},
                               .highlight_top       = {62, 112, 172},
                               .highlight_bottom    = {36, 72, 118},
                               .highlight_text      = {255, 255, 255},
                               .menu_bar_top        = {58, 60, 66},
                               .menu_bar_bottom     = {30, 31, 35},
                               .menu_box_top        = {64, 66, 72},
                               .menu_box_bottom     = {44, 46, 51},
                               .item_top            = {64, 66, 72},
                               .item_bottom         = {50, 52, 57},
                               .separator           = {80, 80, 80},
                               .header              = {45, 45, 45},
                               .pane_border         = {62, 64, 70},
                               .pane_bar            = {34, 35, 38},
                               .focus               = {30, 62, 104},
                               .bubble              = {36, 37, 41}};

        // Light surfaces, near-black text; the blue of what is chosen darker, so white text on it stays readable
        constexpr Palette day{.window              = {236, 236, 233},
                              .metal_top           = {242, 242, 240},
                              .metal_bottom        = {218, 219, 222},
                              .frame               = {120, 122, 128},
                              .outline             = {80, 82, 88},
                              .text                = {12, 12, 14},
                              .text_dim            = {82, 84, 90},
                              .label               = {30, 30, 34},
                              .status              = {60, 60, 64},
                              .button_gray         = {206, 208, 213},
                              .button_blue         = {120, 170, 232},
                              .disabled_background = {226, 226, 226},
                              .disabled_text       = {150, 150, 150},
                              .field               = {255, 255, 255},
                              .field_top           = {255, 255, 255},
                              .field_bottom        = {244, 245, 247},
                              .field_frame         = {130, 130, 130},
                              .groove_top          = {176, 178, 184},
                              .groove_bottom       = {214, 216, 220},
                              .row                 = {255, 255, 255},
                              .row_alternate       = {240, 241, 243},
                              .secondary_text      = {72, 72, 76},
                              .missing_text        = {176, 30, 30},
                              .highlight_top       = {40, 100, 180},
                              .highlight_bottom    = {24, 70, 140},
                              .highlight_text      = {255, 255, 255},
                              .menu_bar_top        = {232, 233, 236},
                              .menu_bar_bottom     = {208, 210, 214},
                              .menu_box_top        = {252, 252, 253},
                              .menu_box_bottom     = {234, 235, 238},
                              .item_top            = {226, 228, 232},
                              .item_bottom         = {212, 214, 218},
                              .separator           = {170, 170, 170},
                              .header              = {214, 216, 220},
                              .pane_border         = {140, 142, 148},
                              .pane_bar            = {218, 220, 224},
                              .focus               = {70, 130, 210},
                              .bubble              = {255, 252, 230}};

        // The look in use; the frontend's thread alone draws, so a plain flag does
        bool use_daylight = false; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables): one look per program
    } // namespace

    const Palette &darkPalette() noexcept
    {
        return dark;
    }

    const Palette &daylightPalette() noexcept
    {
        return day;
    }

    const Palette &colors() noexcept
    {
        return use_daylight ? day : dark;
    }

    void setDaylight(bool on) noexcept
    {
        use_daylight = on;
    }

    bool daylight() noexcept
    {
        return use_daylight;
    }

    Color shade(Color color, int amount) noexcept
    {
        const auto move = [amount](unsigned char value)
        { return static_cast<unsigned char>(std::clamp(static_cast<int>(value) + amount, 0, 255)); };
        return {move(color.red), move(color.green), move(color.blue), color.alpha};
    }

    void background(Canvas &canvas, const Rect &area)
    {
        canvas.fillRoundedRect(area, 0.0F, colors().metal_top, colors().metal_bottom);
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
        canvas.fillRoundedRect(area, radius, colors().outline, colors().outline);
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
        canvas.fillRoundedRect(area, radius, colors().frame, colors().frame);
        canvas.fillRoundedRect(area.inset(1.0F), std::max(0.0F, radius - 1.0F), fill, fill);
    }

    void groove(Canvas &canvas, const Rect &area, bool vertical, float level, Color fill)
    {
        if (area.width <= 0.0F || area.height <= 0.0F)
        {
            return;
        }
        const float radius = std::min(area.width, area.height) / 2;
        canvas.fillRoundedRect(area, radius, colors().outline, colors().outline);
        const Rect inner = area.inset(1.0F);
        canvas.fillRoundedRect(inner, std::max(0.0F, radius - 1.0F), colors().groove_top, colors().groove_bottom);
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

    void knob(Canvas &canvas, const Rect &area, bool held, bool mark_horizontal, std::uint8_t alpha)
    {
        if (area.width <= 0.0F || area.height <= 0.0F)
        {
            return;
        }
        const auto with = [alpha](Color color)
        {
            color.alpha = static_cast<unsigned char>((color.alpha * alpha) / 255);
            return color;
        };
        canvas.fillRoundedRect(area, 4.0F, with(colors().outline), with(colors().outline));
        const Rect body = area.inset(1.0F);
        canvas.fillRoundedRect(body, 3.0F, with(held ? Color{250, 250, 252} : Color{228, 229, 233}),
                               with(held ? Color{150, 152, 158} : Color{116, 118, 124}));
        const Color mark{40, 42, 46, 230};
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

    float meterPosition(float linear) noexcept
    {
        if (!(linear > 0.0F))
        {
            return 0.0F;
        }
        const float decibels = 20.0F * std::log10(linear);
        return std::clamp((decibels - meter_floor_db) / -meter_floor_db, 0.0F, 1.0F);
    }

    void meter(Canvas &canvas, const Rect &area, float left, float right, bool vertical, bool clip)
    {
        if (area.width <= 0.0F || area.height <= 0.0F)
        {
            return;
        }
        constexpr float gap     = 1.0F;
        constexpr float segment = 4.0F; // length of one LED, the gap after it included
        const float     across  = vertical ? area.width : area.height;
        const float     along   = vertical ? area.height : area.width;
        const float     lamp    = std::min(along / 4, std::max(6.0F, across * 0.4F)); // the clip lamp, at the far end
        const float     lanes   = along - lamp - gap;
        const auto      count   = std::max(4, static_cast<int>(lanes / segment));
        const float     step    = lanes / static_cast<float>(count);
        const float     lane    = (across - gap) / 2;

        // In meter coordinates: a runs along the meter from its start, b across it
        const auto place = [&](float a, float b, float length, float width)
        {
            return vertical ? Rect{area.x + b, area.y + area.height - a - length, width, length}
                            : Rect{area.x + a, area.y + b, length, width};
        };
        const float                yellow_from = (meter_yellow_db - meter_floor_db) / -meter_floor_db;
        const float                red_from    = (meter_red_db - meter_floor_db) / -meter_floor_db;
        const std::array<float, 2> levels{meterPosition(left), meterPosition(right)};
        for (std::size_t channel = 0; channel < levels.size(); ++channel)
        {
            const float b = static_cast<float>(channel) * (lane + gap);
            for (int index = 0; index < count; ++index)
            {
                const float at    = static_cast<float>(index) / static_cast<float>(count);
                const bool  lit   = at < levels.at(channel);
                Color       color = at >= red_from      ? Color{230, 50, 40}
                                    : at >= yellow_from ? Color{240, 200, 40}
                                                        : Color{60, 210, 80};
                if (!lit)
                {
                    color = shade(color, -150);
                }
                canvas.fillRect(place((static_cast<float>(index) * step), b, std::max(1.0F, step - gap), lane), color);
            }
        }
        canvas.fillRect(place(along - lamp, 0.0F, lamp, across), clip ? Color{255, 40, 30} : Color{70, 18, 16});
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

    namespace
    {
        /// A straight line of the given thickness, as two triangles
        void line(Canvas &canvas, Point from, Point to, float thickness, Color color)
        {
            const float dx     = to.x - from.x;
            const float dy     = to.y - from.y;
            const float length = std::sqrt((dx * dx) + (dy * dy));
            if (length <= 0.0F)
            {
                return;
            }
            const Point side{-dy / length * thickness / 2, dx / length * thickness / 2};
            const Point a{from.x + side.x, from.y + side.y};
            const Point b{from.x - side.x, from.y - side.y};
            const Point c{to.x - side.x, to.y - side.y};
            const Point d{to.x + side.x, to.y + side.y};
            canvas.fillTriangle(a, b, c, color);
            canvas.fillTriangle(a, c, d, color);
        }
    } // namespace

    void clearFilterIcon(Canvas &canvas, const Rect &area, Color color)
    {
        if (area.width <= 0.0F || area.height <= 0.0F)
        {
            return;
        }
        // The funnel: a wide top narrowing to a stem, in the upper left three quarters
        const float w = area.width;
        const float h = area.height;
        canvas.fillTriangle({area.x, area.y + (h * 0.08F)}, {area.x + (w * 0.74F), area.y + (h * 0.08F)},
                            {area.x + (w * 0.37F), area.y + (h * 0.50F)}, color);
        canvas.fillRect({area.x + (w * 0.31F), area.y + (h * 0.45F), w * 0.12F, h * 0.42F}, color);
        // The cross, at the lower right
        const float thickness = std::max(2.0F, w * 0.11F);
        line(canvas, {area.x + (w * 0.58F), area.y + (h * 0.56F)}, {area.x + (w * 0.96F), area.y + (h * 0.94F)},
             thickness, color);
        line(canvas, {area.x + (w * 0.96F), area.y + (h * 0.56F)}, {area.x + (w * 0.58F), area.y + (h * 0.94F)},
             thickness, color);
    }

    void lanesIcon(Canvas &canvas, const Rect &area, bool vertical, Color color)
    {
        if (area.width <= 0.0F || area.height <= 0.0F)
        {
            return;
        }
        // Two bars with a gap between them, each a lane
        const float w = area.width;
        const float h = area.height;
        for (const float at : {0.12F, 0.56F})
        {
            const Rect lane = vertical ? Rect{area.x + (w * at), area.y, w * 0.32F, h}
                                       : Rect{area.x, area.y + (h * at), w, h * 0.32F};
            canvas.fillRoundedRect(lane, 2.0F, color, color);
        }
    }

    void checkMark(Canvas &canvas, const Rect &area, Color color)
    {
        if (area.width <= 0.0F || area.height <= 0.0F)
        {
            return;
        }
        const float w         = area.width;
        const float h         = area.height;
        const float thickness = std::max(2.0F, w * 0.14F);
        const Point corner{area.x + (w * 0.40F), area.y + (h * 0.80F)};
        line(canvas, {area.x + (w * 0.12F), area.y + (h * 0.52F)}, corner, thickness, color);
        line(canvas, corner, {area.x + (w * 0.90F), area.y + (h * 0.18F)}, thickness, color);
    }
} // namespace djw::player_ui::sdl::look
