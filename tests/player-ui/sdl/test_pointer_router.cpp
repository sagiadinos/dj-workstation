// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "pointer_router.hpp"

#include <doctest/doctest.h>

#include <vector>

using namespace djw::player_ui::sdl;

namespace
{
    /// Records what it receives
    class RecordingWidget final : public Widget
    {
        public:
            explicit RecordingWidget(Rect rect) { setBounds(rect); }

            void pointerDown(Point position) override { events.push_back({'d', position}); }
            void pointerMove(Point position) override { events.push_back({'m', position}); }
            void pointerUp(Point position) override { events.push_back({'u', position}); }
            void draw(Canvas &) const override {}

            struct Received
            {
                    char  kind;
                    Point position;
            };
            std::vector<Received> events;
    };

    constexpr PointerId finger_a{1, 1};
    constexpr PointerId finger_b{1, 2};

    PointerEvent down(PointerId id, float x, float y)
    {
        return {id, PointerPhase::Down, {x, y}};
    }
    PointerEvent move(PointerId id, float x, float y)
    {
        return {id, PointerPhase::Move, {x, y}};
    }
    PointerEvent up(PointerId id, float x, float y)
    {
        return {id, PointerPhase::Up, {x, y}};
    }
} // namespace

TEST_CASE("PointerRouter: a pointer captures the widget it went down on")
{
    RecordingWidget left{{0, 0, 100, 100}};
    RecordingWidget right{{100, 0, 100, 100}};
    PointerRouter   router;
    router.add(left);
    router.add(right);

    router.handle(down(finger_a, 150, 50));

    CHECK(left.events.empty());
    REQUIRE(right.events.size() == 1);
    CHECK(right.events[0].kind == 'd');
    CHECK(router.isCaptured(right));
}

TEST_CASE("PointerRouter: moves and up go to the captured widget even outside its bounds")
{
    RecordingWidget fader{{0, 0, 100, 100}};
    RecordingWidget other{{100, 0, 100, 100}};
    PointerRouter   router;
    router.add(fader);
    router.add(other);

    router.handle(down(finger_a, 50, 50));
    router.handle(move(finger_a, 150, 300));
    router.handle(up(finger_a, 150, 300));

    REQUIRE(fader.events.size() == 3);
    CHECK(fader.events[1].kind == 'm');
    CHECK(fader.events[1].position.y == 300);
    CHECK(fader.events[2].kind == 'u');
    CHECK(other.events.empty());
    CHECK_FALSE(router.isCaptured(fader));
}

TEST_CASE("PointerRouter: two fingers work two widgets at the same time")
{
    RecordingWidget button{{0, 0, 100, 100}};
    RecordingWidget fader{{100, 0, 100, 100}};
    PointerRouter   router;
    router.add(button);
    router.add(fader);

    router.handle(down(finger_a, 150, 10));
    router.handle(down(finger_b, 50, 50));
    router.handle(move(finger_a, 150, 90));
    router.handle(up(finger_b, 50, 50));
    router.handle(move(finger_a, 150, 80));

    CHECK(button.events.size() == 2);
    CHECK(fader.events.size() == 3);
    CHECK(router.isCaptured(fader));
    CHECK_FALSE(router.isCaptured(button));
}

TEST_CASE("PointerRouter: a second finger on a captured widget is ignored")
{
    RecordingWidget fader{{0, 0, 100, 100}};
    PointerRouter   router;
    router.add(fader);

    router.handle(down(finger_a, 50, 50));
    router.handle(down(finger_b, 50, 20));
    router.handle(move(finger_b, 50, 10));
    router.handle(up(finger_b, 50, 10));

    REQUIRE(fader.events.size() == 1);
    CHECK(router.isCaptured(fader));
}

TEST_CASE("PointerRouter: a pointer that went down on empty space captures nothing")
{
    RecordingWidget fader{{0, 0, 100, 100}};
    PointerRouter   router;
    router.add(fader);

    router.handle(down(finger_a, 500, 500));
    router.handle(move(finger_a, 50, 50));
    router.handle(up(finger_a, 50, 50));

    CHECK(fader.events.empty());
}

TEST_CASE("PointerRouter: cancel releases the widget like up")
{
    RecordingWidget fader{{0, 0, 100, 100}};
    PointerRouter   router;
    router.add(fader);

    router.handle(down(finger_a, 50, 50));
    router.handle({finger_a, PointerPhase::Cancel, {50, 50}});
    router.handle(move(finger_a, 50, 10));

    REQUIRE(fader.events.size() == 2);
    CHECK(fader.events[1].kind == 'u');
    CHECK_FALSE(router.isCaptured(fader));
}

TEST_CASE("PointerRouter: overlapping widgets, the one added last is on top")
{
    RecordingWidget below{{0, 0, 100, 100}};
    RecordingWidget above{{50, 50, 100, 100}};
    PointerRouter   router;
    router.add(below);
    router.add(above);

    router.handle(down(finger_a, 75, 75));

    CHECK(below.events.empty());
    CHECK(above.events.size() == 1);
}
