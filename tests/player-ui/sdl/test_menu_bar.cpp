// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "menu_bar.hpp"
#include "pointer_router.hpp"

#include "recording_canvas.hpp"

#include <doctest/doctest.h>

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
    CHECK(fixture.bar.titleArea(1).x == MenuBar::title_width);
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
