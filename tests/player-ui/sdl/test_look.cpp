// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "look.hpp"

#include "recording_canvas.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <tuple>
#include <span>

using namespace djw::player_ui::sdl;

namespace
{
    bool inside(const Rect &inner, const Rect &outer)
    {
        constexpr float eps = 0.01F;
        return inner.x >= outer.x - eps && inner.y >= outer.y - eps &&
               inner.x + inner.width <= outer.x + outer.width + eps &&
               inner.y + inner.height <= outer.y + outer.height + eps;
    }

    bool allInside(const djw::tests::RecordingCanvas &canvas, const Rect &area)
    {
        return std::ranges::all_of(canvas.rounded, [&area](const auto &shape) { return inside(shape.rect, area); }) &&
               std::ranges::all_of(canvas.rects, [&area](const Rect &rect) { return inside(rect, area); });
    }
} // namespace

TEST_CASE("look::shade: moves towards white or black and stays in range")
{
    const Color gray{100, 100, 100, 200};
    CHECK(look::shade(gray, 20).red == 120);
    CHECK(look::shade(gray, -20).blue == 80);
    CHECK(look::shade(gray, 20).alpha == 200);
    CHECK(look::shade(gray, 500).green == 255);
    CHECK(look::shade(gray, -500).green == 0);
}

TEST_CASE("look::raised: an outline and a glossy body inside the area, reversed when pressed, nothing for no area")
{
    const Rect                  area{10, 20, 100, 40};
    djw::tests::RecordingCanvas up;
    look::raised(up, area, look::colors().button_gray, false);
    REQUIRE(up.rounded.size() >= 2);
    CHECK(allInside(up, area));
    const auto &body_up = up.rounded.back();
    CHECK(body_up.top.red > body_up.bottom.red); // light above, dark below

    djw::tests::RecordingCanvas down;
    look::raised(down, area, look::colors().button_gray, true);
    const auto &body_down = down.rounded.back();
    CHECK(body_down.top.red < body_down.bottom.red); // pressed in

    djw::tests::RecordingCanvas nothing;
    look::raised(nothing, {0, 0, 0, 10}, look::colors().button_gray, false);
    CHECK(nothing.rounded.empty());
}

TEST_CASE("look::groove and look::knob stay inside their area, the fill follows the level")
{
    const Rect                  area{0, 0, 16, 200};
    djw::tests::RecordingCanvas half;
    look::groove(half, area, true, 0.5F);
    CHECK(allInside(half, area));
    const Rect fill_half = half.rounded.back().rect;

    djw::tests::RecordingCanvas full;
    look::groove(full, area, true, 1.0F);
    CHECK(full.rounded.back().rect.height > fill_half.height);

    djw::tests::RecordingCanvas empty;
    look::groove(empty, area, true, -1.0F);
    CHECK(empty.rounded.size() == 2); // outline and groove, no fill

    djw::tests::RecordingCanvas knob;
    look::knob(knob, {5, 5, 40, 24}, false, true);
    CHECK(allInside(knob, {5, 5, 40, 24}));
}

TEST_CASE("look::background covers the whole window, sunken and the folder icon stay inside")
{
    const Rect                  window{0, 0, 300, 200};
    djw::tests::RecordingCanvas canvas;
    look::background(canvas, window);
    CHECK(allInside(canvas, window));
    REQUIRE_FALSE(canvas.rounded.empty());
    CHECK(canvas.rounded.front().rect.width == 300.0F);
    CHECK(canvas.rounded.front().rect.height == 200.0F);

    djw::tests::RecordingCanvas panel;
    look::sunken(panel, {10, 10, 80, 30});
    CHECK(allInside(panel, {10, 10, 80, 30}));

    djw::tests::RecordingCanvas icon;
    look::folderIcon(icon, {4, 4, 24, 24}, look::colors().text);
    CHECK(allInside(icon, {4, 4, 24, 24}));
    CHECK_FALSE(icon.rounded.empty());
}

TEST_CASE("look::meterPosition: dB from the floor to full scale, 0 below and for nothing, 1 above")
{
    CHECK(look::meterPosition(1.0F) == doctest::Approx(1.0F));
    CHECK(look::meterPosition(2.0F) == doctest::Approx(1.0F));
    CHECK(look::meterPosition(0.1F) == doctest::Approx(0.5F)); // -20 dB, half way to -40
    CHECK(look::meterPosition(0.01F) == doctest::Approx(0.0F));
    CHECK(look::meterPosition(0.001F) == doctest::Approx(0.0F));
    CHECK(look::meterPosition(0.0F) == 0.0F);
    CHECK(look::meterPosition(-1.0F) == 0.0F);
}

TEST_CASE("look::meter: two lanes of segments lit up to the level, green to yellow to red, the clip lamp at the end")
{
    const auto lit = [](Color color) { return color.red > 150 || color.green > 150; };

    djw::tests::RecordingCanvas silent;
    look::meter(silent, {0, 0, 20, 200}, 0.0F, 0.0F, true, false);
    REQUIRE(silent.rects.size() > 9);
    CHECK(std::ranges::none_of(silent.rect_colors, lit)); // nothing lit, the lamp off

    djw::tests::RecordingCanvas full;
    look::meter(full, {0, 0, 20, 200}, 1.0F, 0.1F, true, true);
    // The lamp: the last rect, at the top, lit red
    const Rect  lamp       = full.rects.back();
    const Color lamp_color = full.rect_colors.back();
    CHECK(lamp.y == doctest::Approx(0.0F));
    CHECK(lamp_color.red > 200);
    CHECK(lamp_color.green < 100);
    // The left lane is lit all the way, its top segment red and its bottom one green; the right one half way
    const std::size_t per_lane = (full.rects.size() - 1) / 2;
    const auto        left     = std::span{full.rect_colors}.first(per_lane);
    const auto        right    = std::span{full.rect_colors}.subspan(per_lane, per_lane);
    CHECK(std::ranges::all_of(left, lit));
    CHECK(left.front().green > left.front().red); // green at the bottom
    CHECK(left.back().red > left.back().green);   // red at the top
    const auto right_lit = std::ranges::count_if(right, lit);
    CHECK(right_lit == doctest::Approx(static_cast<double>(per_lane) / 2).epsilon(0.15));
    // Rising from the bottom: the first segment lies lowest
    CHECK(full.rects.front().y > full.rects[per_lane - 1].y);

    djw::tests::RecordingCanvas sideways;
    look::meter(sideways, {0, 0, 200, 14}, 1.0F, 1.0F, false, false);
    CHECK(sideways.rects.back().x > 150); // the lamp at the right end
}

namespace
{
    /// Switches the daylight colors for a test and back to dark at its end, also when it fails
    struct DaylightOn
    {
            DaylightOn() { look::setDaylight(true); }
            DaylightOn(const DaylightOn &)            = delete;
            DaylightOn &operator=(const DaylightOn &) = delete;
            DaylightOn(DaylightOn &&)                 = delete;
            DaylightOn &operator=(DaylightOn &&)      = delete;
            ~DaylightOn() { look::setDaylight(false); }
    };

    /// The relative luminance of a color (WCAG 2)
    double luminance(Color color)
    {
        const auto channel = [](unsigned char value)
        {
            const double share = value / 255.0;
            return share <= 0.03928 ? share / 12.92 : std::pow((share + 0.055) / 1.055, 2.4);
        };
        return (0.2126 * channel(color.red)) + (0.7152 * channel(color.green)) + (0.0722 * channel(color.blue));
    }

    /// The contrast ratio of two colors (WCAG 2): 1 the same, 21 black on white
    double contrast(Color first, Color second)
    {
        const double light = std::max(luminance(first), luminance(second));
        const double dark  = std::min(luminance(first), luminance(second));
        return (light + 0.05) / (dark + 0.05);
    }

    bool sameColor(Color first, Color second)
    {
        return first.red == second.red && first.green == second.green && first.blue == second.blue &&
               first.alpha == second.alpha;
    }
} // namespace

TEST_CASE("look: dark by default; daylight switches every color the widgets ask for, and back")
{
    CHECK_FALSE(look::daylight());
    CHECK(&look::colors() == &look::darkPalette());
    {
        const DaylightOn day;
        CHECK(look::daylight());
        CHECK(&look::colors() == &look::daylightPalette());
        // The window light, the text dark
        CHECK(luminance(look::colors().metal_top) > 0.8);
        CHECK(luminance(look::colors().text) < 0.01);
    }
    CHECK_FALSE(look::daylight());
    CHECK(luminance(look::colors().metal_top) < 0.05);
}

TEST_CASE("look: in both palettes text stands out from what it is drawn on (WCAG AA, 4.5:1; dim and status 3:1)")
{
    for (const look::Palette *palette : {&look::darkPalette(), &look::daylightPalette()})
    {
        const bool day = palette == &look::daylightPalette();
        CAPTURE(day);
        const look::Palette &colors = *palette;
        // The dark look as it was before the palette misses two of them: light text on the blue of a chosen entry
        // (4.2:1) and the dim shortcuts in an open menu (2.3:1); daylight has to meet them
        if (day)
        {
            CHECK(contrast(colors.highlight_text, colors.highlight_top) >= 4.5);
            CHECK(contrast(colors.text_dim, colors.menu_box_top) >= 3.0);
        }
        for (const auto &[ink, ground, least] : {
                 std::tuple{colors.text, colors.metal_top, 4.5},
                 std::tuple{colors.text, colors.metal_bottom, 4.5},
                 std::tuple{colors.label, colors.metal_bottom, 4.5},
                 std::tuple{colors.text, colors.button_gray, 4.5},
                 std::tuple{colors.text, colors.button_blue, 4.5},
                 std::tuple{colors.text, colors.row, 4.5},
                 std::tuple{colors.text, colors.row_alternate, 4.5},
                 std::tuple{colors.secondary_text, colors.row_alternate, 4.5},
                 std::tuple{colors.text, colors.field_top, 4.5},
                 std::tuple{colors.text, colors.field, 4.5},
                 std::tuple{colors.text, colors.menu_bar_bottom, 4.5},
                 std::tuple{colors.text, colors.menu_box_bottom, 4.5},
                 std::tuple{colors.text, colors.item_bottom, 4.5},
                 std::tuple{colors.text, colors.header, 4.5},
                 std::tuple{colors.text, colors.pane_bar, 4.5},
                 std::tuple{colors.text, colors.bubble, 4.5},
                 std::tuple{colors.missing_text, colors.row, 3.0},
                 std::tuple{colors.status, colors.metal_bottom, 3.0},
                 std::tuple{colors.disabled_text, colors.disabled_background, 2.0},
             })
        {
            CAPTURE(least);
            CHECK(contrast(ink, ground) >= least);
        }
    }
}

TEST_CASE("look: the widgets draw with the palette in use; the displays stay black in daylight")
{
    const DaylightOn            day;
    djw::tests::RecordingCanvas canvas;
    look::background(canvas, {0, 0, 10, 10});
    REQUIRE_FALSE(canvas.rounded.empty());
    CHECK(sameColor(canvas.rounded.front().top, look::daylightPalette().metal_top));
    CHECK(sameColor(canvas.rounded.front().bottom, look::daylightPalette().metal_bottom));

    djw::tests::RecordingCanvas display;
    look::sunken(display, {0, 0, 40, 20});
    REQUIRE(display.rounded.size() == 2);
    CHECK(sameColor(display.rounded.front().top, look::daylightPalette().frame));
    CHECK(sameColor(display.rounded.back().top, look::display_black));
}
