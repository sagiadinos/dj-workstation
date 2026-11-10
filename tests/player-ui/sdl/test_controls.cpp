// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "check_box.hpp"
#include "drop_down.hpp"
#include "pointer_router.hpp"
#include "slider.hpp"
#include "stepper.hpp"
#include "text_button.hpp"
#include "time_format.hpp"

#include "recording_canvas.hpp"

#include <doctest/doctest.h>

using namespace djw::player_ui::sdl;
using djw::tests::RecordingCanvas;

namespace
{
    Point centerOf(const Rect &area)
    {
        return {area.x + (area.width / 2), area.y + (area.height / 2)};
    }

    struct Pointer
    {
            PointerRouter router;

            void down(Point position) { router.handle({{1, 1}, PointerPhase::Down, position}); }
            void move(Point position) { router.handle({{1, 1}, PointerPhase::Move, position}); }
            void up(Point position) { router.handle({{1, 1}, PointerPhase::Up, position}); }
            void tap(Point position)
            {
                down(position);
                up(position);
            }
    };
} // namespace

TEST_CASE("TextButton: down and up edges, held, and a press only when released on the button")
{
    TextButton button{"Go"};
    button.setBounds({0, 0, 100, 40});
    Pointer pointer;
    pointer.router.add(button);

    pointer.down({10, 10});
    CHECK(button.held());
    CHECK(button.takeDown());
    CHECK_FALSE(button.takeDown());
    pointer.up({500, 500}); // let go far away
    CHECK_FALSE(button.held());
    CHECK(button.takeUp());
    CHECK_FALSE(button.takeUp());
    CHECK_FALSE(button.takePress());

    pointer.tap({10, 10});
    CHECK(button.takeDown());
    CHECK(button.takeUp());
    CHECK(button.takePress());
}

TEST_CASE("TextButton: a disabled button takes no input and is drawn dimmed")
{
    TextButton button{"Go"};
    button.setBounds({0, 0, 100, 40});
    Pointer pointer;
    pointer.router.add(button);

    button.setEnabled(false);
    pointer.tap({10, 10});
    CHECK_FALSE(button.takeDown());
    CHECK_FALSE(button.takeUp());
    CHECK_FALSE(button.takePress());
    CHECK_FALSE(button.held());

    button.setEnabled(true);
    pointer.tap({10, 10});
    CHECK(button.takePress());
}

TEST_CASE("TextButton: own colors are used for the background and the label")
{
    struct ColorCanvas final : djw::player_ui::sdl::Canvas
    {
            void  fillRect(const Rect &, Color) override {}
            void  fillRoundedRect(const Rect &, float, Color top, Color bottom) override
            {
                bodies.push_back({top, bottom});
            }
            void  fillTriangle(Point, Point, Point, Color) override {}
            void  text(Point, std::string_view, Color color) override { inks.push_back(color); }
            void  textElided(Point, std::string_view, Color color, float) override { inks.push_back(color); }
            float textHeight() const noexcept override { return 10; }
            float textWidth(std::string_view) const noexcept override { return 10; }
            void  largeText(Point, std::string_view, Color) override {}
            float largeTextHeight() const noexcept override { return 10; }
            float largeTextWidth(std::string_view) const noexcept override { return 10; }
            void  setClip(std::optional<Rect>) override {}
            std::vector<std::pair<Color, Color>> bodies; // top and bottom of every rounded shape
            std::vector<Color>                   inks;
    } canvas;

    TextButton button{"CUE"};
    button.setBounds({0, 0, 100, 40});
    button.setColors(Color{200, 30, 30}, Color{10, 10, 10});
    button.draw(canvas);
    REQUIRE(canvas.bodies.size() == 2); // outline and body
    // The body is a gradient around the color asked for: lighter above, darker below
    CHECK(canvas.bodies.back().first.red > 200);
    CHECK(canvas.bodies.back().second.red < 200);
    REQUIRE(canvas.inks.size() == 1);
    CHECK(canvas.inks[0].red == 10);
}

TEST_CASE("Slider: the value follows the finger from the left to the right end")
{
    Slider slider;
    slider.setBounds({100, 0, 300, 40});
    Pointer pointer;
    pointer.router.add(slider);

    pointer.down({100 + (Slider::knob_width / 2), 20});
    CHECK(slider.value() == doctest::Approx(0.0F));
    CHECK(slider.held());
    CHECK(slider.takeChanged() == false); // it did not move away from 0
    pointer.move({250, 20});
    CHECK(slider.value() == doctest::Approx(0.5F).epsilon(0.02));
    CHECK(slider.takeChanged());
    CHECK_FALSE(slider.takeChanged());
    pointer.move({9'999, 20}); // outside: stops at the end
    CHECK(slider.value() == 1.0F);
    pointer.up({9'999, 20});
    CHECK_FALSE(slider.held());
}

TEST_CASE("Slider: setValue is clamped and is no change by the user")
{
    Slider slider;
    slider.setBounds({0, 0, 200, 40});
    slider.setValue(3.0F);
    CHECK(slider.value() == 1.0F);
    slider.setValue(-1.0F);
    CHECK(slider.value() == 0.0F);
    CHECK_FALSE(slider.takeChanged());
}

TEST_CASE("CheckBox: a tap on it toggles it, sliding off does not")
{
    CheckBox box{"Cue-Load"};
    box.setBounds({0, 0, 200, 30});
    Pointer pointer;
    pointer.router.add(box);

    CHECK_FALSE(box.checked());
    pointer.tap({10, 10});
    CHECK(box.checked());
    CHECK(box.takeToggled());
    CHECK_FALSE(box.takeToggled());
    pointer.tap({150, 10}); // on the label too
    CHECK_FALSE(box.checked());

    pointer.down({10, 10});
    pointer.up({10, 500});
    CHECK_FALSE(box.checked());
    CHECK(box.label() == "Cue-Load");

    static_cast<void>(box.takeToggled());
    box.setChecked(true);
    CHECK(box.checked());
    CHECK_FALSE(box.takeToggled()); // setChecked is no toggle by the user
}

TEST_CASE("Stepper: minus and plus change the value within the limits")
{
    Stepper stepper{1, 3, 2};
    stepper.setBounds({0, 0, 150, 40});
    Pointer pointer;
    pointer.router.add(stepper);

    pointer.tap({10, 20}); // minus
    CHECK(stepper.value() == 1);
    CHECK(stepper.takeChanged());
    pointer.tap({10, 20}); // at the minimum
    CHECK(stepper.value() == 1);
    CHECK_FALSE(stepper.takeChanged());

    pointer.tap({140, 20}); // plus
    pointer.tap({140, 20});
    CHECK(stepper.value() == 3);
    pointer.tap({140, 20}); // at the maximum
    CHECK(stepper.value() == 3);

    pointer.tap({75, 20}); // the number itself does nothing
    CHECK(stepper.value() == 3);
    stepper.setValue(99);
    CHECK(stepper.value() == 3);
    stepper.setValue(-5);
    CHECK(stepper.value() == 1);
}

TEST_CASE("Stepper: draws its value")
{
    Stepper stepper{1, 60, 5};
    stepper.setBounds({0, 0, 150, 40});
    RecordingCanvas canvas;
    stepper.draw(canvas);
    CHECK(canvas.drew("5"));
    CHECK(canvas.drew("-"));
    CHECK(canvas.drew("+"));
}

namespace
{
    struct DropFixture
    {
            DropFixture()
            {
                drop.setItems({"none", "one", "two"});
                drop.setSelected(1);
                drop.place({10, 10, 150, 40}, {0, 0, 400, 300});
                pointer.router.add(drop);
            }

            DropDown drop;
            Pointer  pointer;
    };
} // namespace

TEST_CASE("DropDown: closed it covers only its box and shows the selected entry")
{
    DropFixture fixture;
    CHECK_FALSE(fixture.drop.isOpen());
    CHECK(fixture.drop.bounds().height == 40.0F);
    RecordingCanvas canvas;
    fixture.drop.draw(canvas);
    CHECK(canvas.drew("one"));
    CHECK_FALSE(canvas.drew("two"));
}

TEST_CASE("DropDown: a tap opens the list over the whole window, an entry chosen is reported once and closes it")
{
    DropFixture fixture;
    fixture.pointer.tap(centerOf(fixture.drop.bounds()));
    CHECK(fixture.drop.isOpen());
    CHECK(fixture.drop.bounds().height == 300.0F);

    RecordingCanvas canvas;
    fixture.drop.draw(canvas);
    CHECK(canvas.drew("none"));
    CHECK(canvas.drew("two"));

    fixture.pointer.tap(centerOf(fixture.drop.itemArea(2)));
    CHECK_FALSE(fixture.drop.isOpen());
    CHECK(fixture.drop.takeChosen() == 2);
    CHECK_FALSE(fixture.drop.takeChosen().has_value());
}

TEST_CASE("DropDown: a tap elsewhere or on the box again closes it without choosing")
{
    DropFixture fixture;
    fixture.pointer.tap(centerOf(fixture.drop.bounds()));
    fixture.pointer.tap({390, 290});
    CHECK_FALSE(fixture.drop.isOpen());
    CHECK_FALSE(fixture.drop.takeChosen().has_value());

    fixture.pointer.tap(centerOf(fixture.drop.bounds()));
    fixture.pointer.tap(centerOf(fixture.drop.bounds()));
    CHECK_FALSE(fixture.drop.isOpen());
    CHECK_FALSE(fixture.drop.takeChosen().has_value());
}

TEST_CASE("DropDown: sliding off an entry cancels, an empty list does not open, a vanished selection is dropped")
{
    DropFixture fixture;
    fixture.pointer.tap(centerOf(fixture.drop.bounds()));
    fixture.pointer.down(centerOf(fixture.drop.itemArea(0)));
    fixture.pointer.up(centerOf(fixture.drop.itemArea(1)));
    CHECK_FALSE(fixture.drop.takeChosen().has_value());
    CHECK(fixture.drop.isOpen());

    fixture.drop.setItems({"only"});
    CHECK_FALSE(fixture.drop.selectedIndex().has_value()); // index 1 is gone
    fixture.drop.setItems({});
    CHECK_FALSE(fixture.drop.isOpen());
    fixture.pointer.tap(centerOf(fixture.drop.bounds()));
    CHECK_FALSE(fixture.drop.isOpen());
}

TEST_CASE("DropDown: the list opens above the box when there is no room below")
{
    DropDown drop;
    drop.setItems({"a", "b", "c"});
    drop.place({10, 250, 150, 40}, {0, 0, 400, 300});
    const Rect list = drop.listArea();
    CHECK(list.y + list.height <= 250.0F);
    CHECK(list.y >= 0.0F);
}

TEST_CASE("formatClock: hours, minutes, seconds and hundredths, remaining with a minus")
{
    CHECK(formatClock(0.0, false) == "00:00:00:00");
    CHECK(formatClock(239.0, true) == "-00:03:59:00");
    CHECK(formatClock(3'661.257, false) == "01:01:01:25");
    CHECK(formatClock(59.999, false) == "00:00:59:99"); // truncated, not rounded up
    CHECK(formatClock(-5.0, false) == "00:00:00:00");
    CHECK(formatClock(0.0, true) == "-00:00:00:00");
}
