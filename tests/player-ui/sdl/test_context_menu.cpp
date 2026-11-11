// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "context_menu.hpp"

#include "pointer_router.hpp"
#include "recording_canvas.hpp"

#include <doctest/doctest.h>

using namespace djw::player_ui::sdl;

namespace
{
    struct Fixture
    {
            Fixture()
            {
                menu.place({0, 0, 600, 400});
                menu.setItems({{.id = 7, .label = "Edit tags"},
                               {.id = 8, .label = "Scan again"},
                               {.id = 9, .label = "Off", .enabled = false}});
                router.add(menu);
            }

            void tap(Point position)
            {
                router.handle({{1, 1}, PointerPhase::Down, position});
                router.handle({{1, 1}, PointerPhase::Up, position});
            }

            [[nodiscard]] Point middleOf(std::size_t item) const
            {
                const Rect area = menu.itemArea(item);
                return {area.x + (area.width / 2), area.y + (area.height / 2)};
            }

            ContextMenu   menu;
            PointerRouter router;
    };
} // namespace

TEST_CASE("ContextMenu: closed it catches nothing, open it covers the window")
{
    Fixture fixture;
    CHECK_FALSE(fixture.menu.isOpen());
    CHECK(fixture.menu.bounds().width == 0.0F);

    fixture.menu.open({100, 100});
    CHECK(fixture.menu.isOpen());
    CHECK(fixture.menu.bounds().width == 600.0F);
}

TEST_CASE("ContextMenu: opens at the point, moved inside the window near the edges")
{
    Fixture fixture;
    fixture.menu.open({100, 120});
    CHECK(fixture.menu.menuArea().x == 100.0F);
    CHECK(fixture.menu.menuArea().y == 120.0F);

    fixture.menu.open({590, 395});
    const Rect area = fixture.menu.menuArea();
    CHECK(area.x + area.width <= 600.0F);
    CHECK(area.y + area.height <= 400.0F);
}

TEST_CASE("ContextMenu: tapping an entry chooses it once and closes the menu")
{
    Fixture fixture;
    fixture.menu.open({100, 100});
    fixture.tap(fixture.middleOf(1));

    CHECK(fixture.menu.takeChosen() == 8);
    CHECK_FALSE(fixture.menu.takeChosen().has_value());
    CHECK_FALSE(fixture.menu.isOpen());
}

TEST_CASE("ContextMenu: a tap elsewhere only closes it, a disabled entry cannot be chosen")
{
    Fixture fixture;
    fixture.menu.open({100, 100});
    fixture.tap(fixture.middleOf(2));
    CHECK(fixture.menu.isOpen());
    CHECK_FALSE(fixture.menu.takeChosen().has_value());

    fixture.tap({550, 20});
    CHECK_FALSE(fixture.menu.isOpen());
    CHECK_FALSE(fixture.menu.takeChosen().has_value());
}

TEST_CASE("ContextMenu: pressing one entry and releasing on another chooses nothing")
{
    Fixture fixture;
    fixture.menu.open({100, 100});
    fixture.router.handle({{1, 1}, PointerPhase::Down, fixture.middleOf(0)});
    fixture.router.handle({{1, 1}, PointerPhase::Up, fixture.middleOf(1)});

    CHECK_FALSE(fixture.menu.takeChosen().has_value());
    CHECK(fixture.menu.isOpen());
}

TEST_CASE("ContextMenu: without items it does not open, it draws its labels while open")
{
    ContextMenu empty;
    empty.place({0, 0, 600, 400});
    empty.open({10, 10});
    CHECK_FALSE(empty.isOpen());

    Fixture                     fixture;
    djw::tests::RecordingCanvas canvas;
    fixture.menu.draw(canvas);
    CHECK_FALSE(canvas.drew("Edit tags"));
    fixture.menu.open({10, 10});
    fixture.menu.draw(canvas);
    CHECK(canvas.drew("Edit tags"));
    CHECK(canvas.drew("Scan again"));
}
