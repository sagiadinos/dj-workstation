// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "control/controller.hpp"

#include <doctest/doctest.h>

#include <optional>
#include <vector>

using namespace djw::core::control;

namespace
{
    Control control(MidiKind kind, int channel, std::uint8_t number, Action action,
                    std::optional<std::size_t> deck = std::nullopt, Mode mode = Mode::Button, bool shift = false)
    {
        return {.kind    = kind,
                .channel = channel,
                .number  = number,
                .action  = action,
                .deck    = deck,
                .mode    = mode,
                .shift   = shift,
                .invert  = false};
    }

    Mapping testMapping()
    {
        Mapping mapping{.name = "Test", .match = {"Test"}, .audio = {}, .controls = {}, .file = {}};
        mapping.controls = {
            control(MidiKind::Note, 1, 3, Action::Shift),
            control(MidiKind::Note, 2, 7, Action::DeckPlay, 0),
            control(MidiKind::Note, 2, 6, Action::DeckCue, 0),
            control(MidiKind::Note, 2, 6, Action::DeckLoad, 0, Mode::Button, true),
            control(MidiKind::ControlChange, 1, 0, Action::Crossfader, std::nullopt, Mode::Absolute),
            control(MidiKind::ControlChange, 2, 8, Action::DeckPitch, 0, Mode::Absolute14),
            control(MidiKind::ControlChange, 1, 1, Action::LibraryMove, std::nullopt, Mode::Relative),
        };
        return mapping;
    }

    /// A button event of player 1
    ControlEvent event(Action action, bool pressed)
    {
        return {.action = action, .deck = 0, .pressed = pressed, .value = 0.0F, .steps = 0};
    }

    MidiMessage noteOn(int channel, std::uint8_t note, std::uint8_t velocity = 127)
    {
        return {.status = static_cast<std::uint8_t>(0x90 + channel - 1), .data1 = note, .data2 = velocity};
    }
    MidiMessage noteOff(int channel, std::uint8_t note)
    {
        return {.status = static_cast<std::uint8_t>(0x80 + channel - 1), .data1 = note, .data2 = 0};
    }
    MidiMessage cc(int channel, std::uint8_t number, std::uint8_t value)
    {
        return {.status = static_cast<std::uint8_t>(0xB0 + channel - 1), .data1 = number, .data2 = value};
    }
} // namespace

TEST_CASE("Controller: a button gives pressed and released, a note on with velocity 0 counts as released")
{
    Controller controller{testMapping()};
    CHECK(controller.handle(noteOn(2, 7)) == std::vector{event(Action::DeckPlay, true)});
    CHECK(controller.handle(noteOn(2, 7, 0)) == std::vector{event(Action::DeckPlay, false)});
    CHECK(controller.handle(noteOff(2, 7)).empty());    // released twice: nothing more
    CHECK(controller.handle(noteOn(5, 7)).empty());     // another channel: not mapped
    CHECK(controller.handle({.status = 0xF8}).empty()); // timing clock
}

TEST_CASE("Controller: Shift switches to the shifted controls; a button is let go as what it was pressed as")
{
    Controller controller{testMapping()};
    CHECK(controller.handle(noteOn(2, 6)).front().action == Action::DeckCue);
    CHECK(controller.handle(noteOn(1, 3)).empty()); // Shift down
    CHECK(controller.shiftHeld());
    // CUE was pressed without Shift: it is released as CUE
    CHECK(controller.handle(noteOff(2, 6)) == std::vector{event(Action::DeckCue, false)});
    // With Shift the same pad loads
    CHECK(controller.handle(noteOn(2, 6)).front().action == Action::DeckLoad);
    // Not shifted controls have no shifted version: nothing
    CHECK(controller.handle(noteOn(2, 7)).empty());
    CHECK(controller.handle(noteOff(1, 3)).empty()); // Shift up
    CHECK_FALSE(controller.shiftHeld());
    CHECK(controller.handle(noteOff(2, 6)).front() == event(Action::DeckLoad, false));
}

TEST_CASE("Controller: absolute 0 to 1, 14 bits from the high and low parts, relative steps both ways")
{
    Controller controller{testMapping()};
    CHECK(controller.handle(cc(1, 0, 127)).front().value == doctest::Approx(1.0F));
    CHECK(controller.handle(cc(1, 0, 0)).front().value == doctest::Approx(0.0F));

    // High part 64, low part 0: the middle; then the low part refines it
    CHECK(controller.handle(cc(2, 8, 64)).front().value == doctest::Approx(8192.0F / 16383.0F));
    const auto refined = controller.handle(cc(2, 8 + 32, 127));
    REQUIRE(refined.size() == 1);
    CHECK(refined.front().action == Action::DeckPitch);
    CHECK(refined.front().value == doctest::Approx((8192.0F + 127.0F) / 16383.0F));
    CHECK(controller.handle(cc(2, 8, 127)).front().value == doctest::Approx(1.0F));

    CHECK(controller.handle(cc(1, 1, 1)).front().steps == 1);
    CHECK(controller.handle(cc(1, 1, 3)).front().steps == 3);
    CHECK(controller.handle(cc(1, 1, 127)).front().steps == -1);
    CHECK(controller.handle(cc(1, 1, 125)).front().steps == -3);
    CHECK(controller.handle(cc(1, 1, 0)).empty());
}

TEST_CASE("Controller: an inverted absolute control runs the other way")
{
    Mapping mapping{.name = "Test", .match = {"Test"}, .audio = {}, .controls = {}, .file = {}};
    Control pitch    = control(MidiKind::ControlChange, 1, 8, Action::DeckPitch, 0, Mode::Absolute14);
    pitch.invert     = true;
    Control fader    = control(MidiKind::ControlChange, 1, 0, Action::DeckVolume, 0, Mode::Absolute);
    fader.invert     = true;
    mapping.controls = {pitch, fader};
    Controller controller{mapping};
    CHECK(controller.handle(cc(1, 8, 127)).front().value == doctest::Approx(0.0F).epsilon(0.01));
    CHECK(controller.handle(cc(1, 0, 0)).front().value == doctest::Approx(1.0F));
}

TEST_CASE("Controller: a cc button is released with value 0, a note off with any velocity releases a note")
{
    Mapping mapping{.name = "Test", .match = {"Test"}, .audio = {}, .controls = {}, .file = {}};
    mapping.controls = {control(MidiKind::ControlChange, 1, 20, Action::DeckPlay, 0),
                        control(MidiKind::Note, 1, 21, Action::DeckCue, 0)};
    Controller controller{mapping};
    CHECK(controller.handle(cc(1, 20, 127)) == std::vector{event(Action::DeckPlay, true)});
    CHECK(controller.handle(cc(1, 20, 0)) == std::vector{event(Action::DeckPlay, false)});
    CHECK(controller.handle(noteOn(1, 21)) == std::vector{event(Action::DeckCue, true)});
    const MidiMessage off_with_velocity{.status = 0x80, .data1 = 21, .data2 = 64};
    CHECK(controller.handle(off_with_velocity) == std::vector{event(Action::DeckCue, false)});
}

TEST_CASE("Controller: the low part of a 14-bit control at cc 0 is cc 32, it replaces the low bits only")
{
    Mapping mapping{.name = "Test", .match = {"Test"}, .audio = {}, .controls = {}, .file = {}};
    mapping.controls = {control(MidiKind::ControlChange, 3, 0, Action::DeckPitch, 0, Mode::Absolute14)};
    Controller controller{mapping};
    CHECK(controller.handle(cc(3, 0, 64)).front().value == doctest::Approx(8192.0F / 16383.0F));
    const auto low = controller.handle(cc(3, 32, 5));
    REQUIRE(low.size() == 1);
    CHECK(low.front().value == doctest::Approx((8192.0F + 5.0F) / 16383.0F));
    CHECK(controller.handle(cc(3, 32, 9)).front().value == doctest::Approx((8192.0F + 9.0F) / 16383.0F));
}
