// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "menu_bar.hpp"
#include "pointer_router.hpp"

#include "recording_canvas.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <string_view>

using namespace djw::player_ui::sdl;
using djw::tests::RecordingCanvas;

namespace
{
    struct Fixture
    {
            explicit Fixture(bool right_to_left = false)
            {
                bar.setRightToLeft(right_to_left);
                bar.setMenus({{"File", {{.id = 1, .label = "Open"}, {.id = 2, .label = "Save", .enabled = false}}},
                              {"Edit", {{.id = 3, .label = "Undo", .checked = true}}}});
                bar.place({0, 0, 600, MenuBar::bar_height}, {0, 0, 600, 400});
                router.add(bar);
            }

            void tap(Point position)
            {
                router.handle({{1, 1}, PointerPhase::Down, position});
                router.handle({{1, 1}, PointerPhase::Up, position});
            }

            void tapTitle(std::size_t menu)
            {
                const Rect area = bar.titleArea(menu);
                tap({area.x + 10, area.y + 10});
            }

            void tapItem(std::size_t menu, std::size_t item)
            {
                const Rect area = bar.itemArea(menu, item);
                tap({area.x + 10, area.y + 10});
            }

            MenuBar       bar;
            PointerRouter router;
    };
} // namespace

TEST_CASE("MenuBar: closed it covers only the bar")
{
    Fixture fixture;
    CHECK_FALSE(fixture.bar.isOpen());
    CHECK(fixture.bar.bounds().height == MenuBar::bar_height);
    CHECK(fixture.bar.titleArea(0).x == 0.0F);
    CHECK(fixture.bar.titleArea(1).x == MenuBar::min_title_width); // not measured yet
}

TEST_CASE("MenuBar: a title is as wide as its text with a margin, at least the least width; too wide together the "
          "widest are cut")
{
    MenuBar bar;
    bar.setMenus({{"Fi", {}}, {"A rather long title", {}}, {"View", {}}});
    bar.place({0, 0, 600, MenuBar::bar_height}, {0, 0, 600, 400});
    RecordingCanvas canvas;
    bar.draw(canvas);
    const float long_text = canvas.textWidth("A rather long title");
    CHECK(bar.titleArea(0).width == MenuBar::min_title_width);
    CHECK(bar.titleArea(1).x == MenuBar::min_title_width);
    CHECK(bar.titleArea(1).width > long_text);
    CHECK(bar.titleArea(1).width < long_text + 40.0F);
    CHECK(bar.titleArea(2).x == bar.titleArea(1).x + bar.titleArea(1).width);

    const float view = bar.titleArea(2).width;
    bar.place({0, 0, 250, MenuBar::bar_height}, {0, 0, 250, 400});
    const Rect last = bar.titleArea(2);
    CHECK(last.x + last.width == doctest::Approx(250.0F));
    CHECK(bar.titleArea(0).width == MenuBar::min_title_width); // the short ones stay whole
    CHECK(last.width == view);
    CHECK(bar.titleArea(1).width == doctest::Approx(250.0F - MenuBar::min_title_width - view));
    CHECK(bar.titleArea(5).width == 0.0F);
}

TEST_CASE("MenuBar: tapping a title opens its menu over the whole window, tapping it again closes it")
{
    Fixture fixture;
    fixture.tapTitle(0);
    CHECK(fixture.bar.openMenu() == 0);
    CHECK(fixture.bar.bounds().height == 400.0F);
    fixture.tapTitle(0);
    CHECK_FALSE(fixture.bar.isOpen());
    CHECK(fixture.bar.bounds().height == MenuBar::bar_height);
}

TEST_CASE("MenuBar: another title switches the menu")
{
    Fixture fixture;
    fixture.tapTitle(0);
    fixture.tapTitle(1);
    CHECK(fixture.bar.openMenu() == 1);
}

TEST_CASE("MenuBar: choosing an entry reports its id once and closes the menu")
{
    Fixture fixture;
    fixture.tapTitle(0);
    fixture.tapItem(0, 0);
    CHECK_FALSE(fixture.bar.isOpen());
    CHECK(fixture.bar.takeChosen() == 1);
    CHECK_FALSE(fixture.bar.takeChosen().has_value());
}

TEST_CASE("MenuBar: a disabled entry cannot be chosen and keeps the menu open")
{
    Fixture fixture;
    fixture.tapTitle(0);
    fixture.tapItem(0, 1);
    CHECK_FALSE(fixture.bar.takeChosen().has_value());
    CHECK(fixture.bar.isOpen());
}

TEST_CASE("MenuBar: a tap elsewhere closes the menu and nothing else gets it")
{
    Fixture fixture;
    fixture.tapTitle(0);
    fixture.tap({500, 300});
    CHECK_FALSE(fixture.bar.isOpen());
    CHECK_FALSE(fixture.bar.takeChosen().has_value());
}

TEST_CASE("MenuBar: sliding off the entry cancels the press")
{
    Fixture fixture;
    fixture.tapTitle(0);
    const Rect item = fixture.bar.itemArea(0, 0);
    fixture.router.handle({{1, 1}, PointerPhase::Down, {item.x + 10, item.y + 10}});
    fixture.router.handle({{1, 1}, PointerPhase::Up, {item.x + 10, item.y + 100}});
    CHECK_FALSE(fixture.bar.takeChosen().has_value());
}

TEST_CASE("MenuBar: menus that vanish close an open menu")
{
    Fixture fixture;
    fixture.tapTitle(1);
    fixture.bar.setMenus({{"Only", {{.id = 9, .label = "x"}}}});
    CHECK_FALSE(fixture.bar.isOpen());
}

TEST_CASE("MenuBar: draws the titles and the open menu's entries")
{
    Fixture         fixture;
    RecordingCanvas closed;
    fixture.bar.draw(closed);
    CHECK(closed.drew("File"));
    CHECK(closed.drew("Edit"));
    CHECK_FALSE(closed.drew("Open"));

    fixture.tapTitle(0);
    RecordingCanvas open;
    fixture.bar.draw(open);
    CHECK(open.drew("Open"));
    CHECK(open.drew("Save"));
    CHECK_FALSE(open.drew("Undo"));
}

TEST_CASE("MenuBar: right to left starts at the right edge and aligns the drop-down to its title")
{
    Fixture    fixture{true};
    const Rect first  = fixture.bar.titleArea(0);
    const Rect second = fixture.bar.titleArea(1);
    CHECK(first.x + first.width == 600.0F);
    CHECK(second.x + second.width == first.x);

    fixture.tapTitle(0);
    const Rect dropdown = fixture.bar.dropdownArea(0);
    CHECK(dropdown.x + dropdown.width == first.x + first.width);
    fixture.tapItem(0, 0);
    CHECK(fixture.bar.takeChosen() == 1);
}

TEST_CASE("MenuBar: the drop-down stays inside the window")
{
    Fixture fixture;
    fixture.bar.place({0, 0, 600, MenuBar::bar_height}, {0, 0, 260, 400});
    fixture.tapTitle(1);
    const Rect dropdown = fixture.bar.dropdownArea(1);
    CHECK(dropdown.x >= 0.0F);
    CHECK(dropdown.x + dropdown.width <= 260.0F);
}

TEST_CASE("MenuBar: an entry shows its shortcut at the end, at the start in right-to-left layout")
{
    for (const bool right_to_left : {false, true})
    {
        MenuBar bar;
        bar.setRightToLeft(right_to_left);
        bar.setMenus({{"File", {{.id = 1, .label = "New", .shortcut = "Ctrl+N"}}}});
        bar.place({0, 0, 600, MenuBar::bar_height}, {0, 0, 600, 400});
        PointerRouter router;
        router.add(bar);
        const Rect title = bar.titleArea(0);
        router.handle({{1, 1}, PointerPhase::Down, {title.x + 10, title.y + 10}});
        router.handle({{1, 1}, PointerPhase::Up, {title.x + 10, title.y + 10}});

        RecordingCanvas canvas;
        bar.draw(canvas);
        float shortcut_x = -1;
        float label_x    = -1;
        for (const auto &drawn : canvas.texts)
        {
            shortcut_x = drawn.text == "Ctrl+N" ? drawn.position.x : shortcut_x;
            label_x    = drawn.text == "New" ? drawn.position.x : label_x;
        }
        REQUIRE(shortcut_x >= 0);
        REQUIRE(label_x >= 0);
        CHECK((right_to_left ? shortcut_x < label_x : shortcut_x > label_x));
    }
}

TEST_CASE("MenuBar: the drop-down is one row per entry under the bar; right to left it stays inside the window too")
{
    Fixture fixture{true};
    fixture.bar.place({0, 0, 150, MenuBar::bar_height}, {0, 0, 600, 400});
    fixture.tapTitle(1);
    const Rect dropdown = fixture.bar.dropdownArea(1);
    CHECK(dropdown.x == 0.0F); // it would start before the window
    CHECK(dropdown.y == MenuBar::bar_height);
    CHECK(fixture.bar.dropdownArea(0).height == 2 * MenuBar::item_height);
    CHECK(dropdown.height == MenuBar::item_height);
}

TEST_CASE("MenuBar: an entry set apart has a line at its top, the first entry none; the heights stay")
{
    MenuBar bar;
    bar.setMenus(
        {{"File", {{.id = 1, .label = "Open", .separated = true}, {.id = 2, .label = "Quit", .separated = true}}}});
    bar.place({0, 0, 600, MenuBar::bar_height}, {0, 0, 600, 400});
    PointerRouter router;
    router.add(bar);
    const Rect title = bar.titleArea(0);
    router.handle({{1, 1}, PointerPhase::Down, {title.x + 10, title.y + 10}});
    router.handle({{1, 1}, PointerPhase::Up, {title.x + 10, title.y + 10}});
    REQUIRE(bar.isOpen());

    RecordingCanvas canvas;
    bar.draw(canvas);
    const auto line_at = [&canvas](float y)
    { return std::ranges::any_of(canvas.rects, [y](const Rect &rect) { return rect.y == y && rect.height == 1.0F; }); };
    CHECK(line_at(bar.itemArea(0, 1).y));
    CHECK_FALSE(line_at(bar.itemArea(0, 0).y));
    CHECK(bar.itemArea(0, 1).height == MenuBar::item_height);
}

TEST_CASE("MenuBar: an entry that is on has a check mark at its start, no colored band; the labels line up after it")
{
    for (const bool right_to_left : {false, true})
    {
        CAPTURE(right_to_left);
        Fixture fixture{right_to_left};
        fixture.bar.setMenus({{"Edit",
                               {{.id = 3, .label = "Undo", .checked = true},
                                {.id = 4, .label = "Redo"},
                                {.id = 5, .label = "Off", .enabled = false, .checked = true}}}});
        fixture.tapTitle(0);
        RecordingCanvas canvas;
        fixture.bar.draw(canvas);

        const Rect undo     = fixture.bar.itemArea(0, 0);
        const Rect redo     = fixture.bar.itemArea(0, 1);
        const auto marks_in = [&canvas](const Rect &entry)
        {
            return std::ranges::count_if(canvas.triangles, [&entry](const RecordingCanvas::DrawnTriangle &triangle)
                                         { return entry.contains(triangle.first) && entry.contains(triangle.third); });
        };
        CHECK(marks_in(undo) > 0);
        CHECK(marks_in(redo) == 0);
        CHECK(marks_in(fixture.bar.itemArea(0, 2)) > 0); // off, but on: the mark dimmed
        // No band over the whole entry
        CHECK_FALSE(std::ranges::any_of(
            canvas.rounded, [&undo](const auto &drawn)
            { return drawn.rect.y == undo.y && drawn.rect.width == undo.width && drawn.rect.height == undo.height; }));
        // The mark at the start: in the first or (right to left) the last third of the entry
        const auto mark = std::ranges::find_if(canvas.triangles, [&undo](const RecordingCanvas::DrawnTriangle &triangle)
                                               { return undo.contains(triangle.first); });
        REQUIRE(mark != canvas.triangles.end());
        if (right_to_left)
        {
            CHECK(mark->first.x > undo.x + (undo.width * 2 / 3));
        }
        else
        {
            CHECK(mark->first.x < undo.x + (undo.width / 3));
        }
        // Both labels start (or end) at the same place, checked or not
        const auto label = [&canvas](std::string_view text)
        {
            const auto found = std::ranges::find(canvas.texts, text, &RecordingCanvas::DrawnText::text);
            REQUIRE(found != canvas.texts.end());
            return *found;
        };
        if (right_to_left)
        {
            CHECK(label("Undo").position.x + (4 * RecordingCanvas::char_width) ==
                  doctest::Approx(label("Redo").position.x + (4 * RecordingCanvas::char_width)));
        }
        else
        {
            CHECK(label("Undo").position.x == doctest::Approx(label("Redo").position.x));
            CHECK(label("Undo").position.x > mark->first.x);
        }
    }
}

TEST_CASE("MenuBar: the note stands at the end of the bar while the titles leave room, else it is left out")
{
    const auto drawnNote = [](const RecordingCanvas &canvas) -> const RecordingCanvas::DrawnText *
    {
        const auto found =
            std::ranges::find(canvas.texts, std::string{"1.0.0-dev.42"}, &RecordingCanvas::DrawnText::text);
        return found != canvas.texts.end() ? &*found : nullptr;
    };

    Fixture fixture;
    fixture.bar.setNote("1.0.0-dev.42");
    RecordingCanvas canvas;
    fixture.bar.draw(canvas);
    const auto *note = drawnNote(canvas);
    REQUIRE(note != nullptr);
    const float width = canvas.textWidth("1.0.0-dev.42");
    CHECK(note->position.x + width <= 600.0F);
    CHECK(note->position.x + width > 600.0F - 30.0F); // at the right end
    CHECK(note->position.x > fixture.bar.titleArea(1).x + fixture.bar.titleArea(1).width);

    Fixture rtl{true};
    rtl.bar.setNote("1.0.0-dev.42");
    RecordingCanvas rtl_canvas;
    rtl.bar.draw(rtl_canvas);
    const auto *rtl_note = drawnNote(rtl_canvas);
    REQUIRE(rtl_note != nullptr);
    CHECK(rtl_note->position.x < 30.0F); // at the left end
    CHECK(rtl_note->position.x + width < rtl.bar.titleArea(1).x);

    // Too narrow: the titles fill the bar, the note is left out instead of covering one
    fixture.bar.place({0, 0, 140, MenuBar::bar_height}, {0, 0, 140, 400});
    RecordingCanvas narrow;
    fixture.bar.draw(narrow);
    CHECK(drawnNote(narrow) == nullptr);
}
