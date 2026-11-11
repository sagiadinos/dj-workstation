// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "sdl_events.hpp"

#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_touch.h>

#include <doctest/doctest.h>

using namespace djw::player_ui::sdl;

namespace
{
    SDL_Event mouseButton(SDL_EventType type, SDL_MouseID which, Uint8 button, float x, float y)
    {
        SDL_Event event{};
        event.type          = type;
        event.button.which  = which;
        event.button.button = button;
        event.button.x      = x;
        event.button.y      = y;
        return event;
    }

    SDL_Event finger(SDL_EventType type, SDL_TouchID touch, SDL_FingerID id, float x, float y)
    {
        SDL_Event event{};
        event.type             = type;
        event.tfinger.touchID  = touch;
        event.tfinger.fingerID = id;
        event.tfinger.x        = x;
        event.tfinger.y        = y;
        return event;
    }
} // namespace

TEST_CASE("toPointerEvent: left mouse button down and up become mouse pointer events")
{
    const auto down = toPointerEvent(mouseButton(SDL_EVENT_MOUSE_BUTTON_DOWN, 1, SDL_BUTTON_LEFT, 10, 20), 100, 100);
    REQUIRE(down.has_value());
    CHECK(down->pointer == mouse_pointer);
    CHECK(down->phase == PointerPhase::Down);
    CHECK(down->position.x == 10);
    CHECK(down->position.y == 20);

    const auto up = toPointerEvent(mouseButton(SDL_EVENT_MOUSE_BUTTON_UP, 1, SDL_BUTTON_LEFT, 10, 20), 100, 100);
    REQUIRE(up.has_value());
    CHECK(up->phase == PointerPhase::Up);
}

TEST_CASE("toPointerEvent: other mouse buttons are ignored")
{
    CHECK_FALSE(toPointerEvent(mouseButton(SDL_EVENT_MOUSE_BUTTON_DOWN, 1, SDL_BUTTON_RIGHT, 1, 1), 100, 100));
}

TEST_CASE("toPointerEvent: mouse events synthesized from touch are dropped")
{
    CHECK_FALSE(
        toPointerEvent(mouseButton(SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_TOUCH_MOUSEID, SDL_BUTTON_LEFT, 1, 1), 100, 100));
}

TEST_CASE("toPointerEvent: fingers get their own pointer and window coordinates")
{
    const auto down = toPointerEvent(finger(SDL_EVENT_FINGER_DOWN, 7, 3, 0.5F, 0.25F), 400, 800);
    REQUIRE(down.has_value());
    CHECK(down->pointer == PointerId{7, 3});
    CHECK(down->phase == PointerPhase::Down);
    CHECK(down->position.x == doctest::Approx(200));
    CHECK(down->position.y == doctest::Approx(200));

    CHECK(toPointerEvent(finger(SDL_EVENT_FINGER_MOTION, 7, 3, 0, 0), 1, 1)->phase == PointerPhase::Move);
    CHECK(toPointerEvent(finger(SDL_EVENT_FINGER_UP, 7, 3, 0, 0), 1, 1)->phase == PointerPhase::Up);
    CHECK(toPointerEvent(finger(SDL_EVENT_FINGER_CANCELED, 7, 3, 0, 0), 1, 1)->phase == PointerPhase::Cancel);
}

TEST_CASE("toPointerEvent: touch events synthesized from the mouse are dropped")
{
    CHECK_FALSE(toPointerEvent(finger(SDL_EVENT_FINGER_DOWN, SDL_MOUSE_TOUCHID, 1, 0, 0), 100, 100));
}

TEST_CASE("toPointerEvent: other events are ignored")
{
    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    CHECK_FALSE(toPointerEvent(event, 100, 100));
}

TEST_CASE("toKeyboardInput: typed text and the editing keys, nothing else")
{
    SDL_Event text{};
    text.type            = SDL_EVENT_TEXT_INPUT;
    const char typed[]   = "é";
    text.text.text       = typed;
    const auto from_text = toKeyboardInput(text);
    REQUIRE(from_text.has_value());
    CHECK(std::get<std::string>(*from_text) == "é");

    const auto key = [](SDL_Keycode code)
    {
        SDL_Event event{};
        event.type    = SDL_EVENT_KEY_DOWN;
        event.key.key = code;
        return toKeyboardInput(event);
    };
    CHECK(key(SDLK_BACKSPACE) == KeyboardInput{EditKey::Backspace});
    CHECK(key(SDLK_RETURN) == KeyboardInput{EditKey::Enter});
    CHECK(key(SDLK_KP_ENTER) == KeyboardInput{EditKey::Enter});
    CHECK(key(SDLK_ESCAPE) == KeyboardInput{EditKey::Escape});
    CHECK_FALSE(key(SDLK_A).has_value());

    SDL_Event up{};
    up.type    = SDL_EVENT_KEY_UP;
    up.key.key = SDLK_BACKSPACE;
    CHECK_FALSE(toKeyboardInput(up).has_value());
}

TEST_CASE("toWheelEvent: position and direction, flipped wheels turned back")
{
    SDL_Event event{};
    event.type            = SDL_EVENT_MOUSE_WHEEL;
    event.wheel.mouse_x   = 10;
    event.wheel.mouse_y   = 20;
    event.wheel.y         = 2;
    event.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
    const auto normal     = toWheelEvent(event);
    REQUIRE(normal.has_value());
    CHECK(normal->position.x == 10);
    CHECK(normal->position.y == 20);
    CHECK(normal->dy == 2);

    event.wheel.direction = SDL_MOUSEWHEEL_FLIPPED;
    CHECK(toWheelEvent(event)->dy == -2);

    CHECK_FALSE(toWheelEvent(SDL_Event{}).has_value());
}

TEST_CASE("toSecondaryClick: the right mouse button going down gives its position, nothing else does")
{
    const auto click = toSecondaryClick(mouseButton(SDL_EVENT_MOUSE_BUTTON_DOWN, 1, SDL_BUTTON_RIGHT, 12, 34));
    REQUIRE(click.has_value());
    CHECK(click->x == 12.0F);
    CHECK(click->y == 34.0F);

    CHECK_FALSE(toSecondaryClick(mouseButton(SDL_EVENT_MOUSE_BUTTON_UP, 1, SDL_BUTTON_RIGHT, 12, 34)));
    CHECK_FALSE(toSecondaryClick(mouseButton(SDL_EVENT_MOUSE_BUTTON_DOWN, 1, SDL_BUTTON_LEFT, 12, 34)));
    CHECK_FALSE(
        toSecondaryClick(mouseButton(SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_TOUCH_MOUSEID, SDL_BUTTON_RIGHT, 12, 34)));
}
