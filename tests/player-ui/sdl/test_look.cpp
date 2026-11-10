// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "look.hpp"

#include "recording_canvas.hpp"

#include <doctest/doctest.h>

#include <algorithm>

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
    look::raised(up, area, look::button_gray, false);
    REQUIRE(up.rounded.size() >= 2);
    CHECK(allInside(up, area));
    const auto &body_up = up.rounded.back();
    CHECK(body_up.top.red > body_up.bottom.red); // light above, dark below

    djw::tests::RecordingCanvas down;
    look::raised(down, area, look::button_gray, true);
    const auto &body_down = down.rounded.back();
    CHECK(body_down.top.red < body_down.bottom.red); // pressed in

    djw::tests::RecordingCanvas nothing;
    look::raised(nothing, {0, 0, 0, 10}, look::button_gray, false);
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
    look::folderIcon(icon, {4, 4, 24, 24}, look::text);
    CHECK(allInside(icon, {4, 4, 24, 24}));
    CHECK_FALSE(icon.rounded.empty());
}
