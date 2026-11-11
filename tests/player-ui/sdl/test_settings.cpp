// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "devices_dialog.hpp"
#include "pointer_router.hpp"
#include "recording_canvas.hpp"
#include "settings.hpp"

#include <doctest/doctest.h>

#include <string>

using namespace djw::player_ui::sdl;
using djw::tests::RecordingCanvas;

TEST_CASE("Settings: the latency comes back as saved; anything unreadable keeps the default")
{
    const Settings saved{.buffer_frames = 512};
    CHECK(parseSettings(formatSettings(saved)) == saved);
    CHECK(parseSettings("").buffer_frames == default_buffer_frames);
    CHECK(parseSettings("something else\nbuffer-frames 512\n").buffer_frames == default_buffer_frames);
    CHECK(parseSettings("djw-settings 1\nbuffer-frames 300\n").buffer_frames == default_buffer_frames); // not offered
    CHECK(parseSettings("djw-settings 1\nbuffer-frames 5x\n").buffer_frames == default_buffer_frames);
    // Lines of a later version are skipped
    CHECK(parseSettings("djw-settings 1\ndriver alsa\nbuffer-frames 1024\n").buffer_frames == 1024);
}

TEST_CASE("Settings: the audio output, its channels and the controllers' mappings come back as saved")
{
    Settings saved;
    saved.output_device    = "DJControl Starlight Analog Surround 4.0";
    saved.headphone_output = true;
    saved.controllers      = {{"DJControl Starlight:DJControl Starlight MIDI 1", "Hercules DJControl Starlight"},
                              {"Pad", ""}};
    CHECK(parseSettings(formatSettings(saved)) == saved);
    // Automatic and stereo by default
    const Settings plain = parseSettings("djw-settings 1\n");
    CHECK(plain.output_device.empty());
    CHECK_FALSE(plain.headphone_output);
    CHECK(plain.controllers.empty());
    // A controller line without its tab is skipped
    CHECK(parseSettings("djw-settings 1\ncontroller Pad only\n").controllers.empty());
}

namespace
{
    struct DialogFixture
    {
            explicit DialogFixture(Language language = Language::English)
            {
                translator.setLanguage(language);
                dialog.layout(window);
                dialog.addTo(router);
                dialog.update();
            }

            void tap(Point position)
            {
                router.handle({{1, 1}, PointerPhase::Down, position});
                router.handle({{1, 1}, PointerPhase::Up, position});
                dialog.update();
            }

            static Point middle(const Rect &area) { return {area.x + (area.width / 2), area.y + (area.height / 2)}; }

            static constexpr Rect window{0, 0, 1000, 700};
            Translator            translator;
            DevicesDialog         dialog{translator, Settings{.buffer_frames = 256}, 48'000};
            PointerRouter         router;
    };
} // namespace

TEST_CASE("DevicesDialog: the latencies in milliseconds at the sample rate, in the user's language")
{
    DialogFixture english;
    CHECK(english.dialog.latencyText(0) == "Shortest (2.7 ms)");
    CHECK(english.dialog.latencyText(1) == "Short (5.3 ms)");
    CHECK(english.dialog.latencyText(3) == "Safe (21.3 ms)");
    DialogFixture german{Language::German};
    CHECK(german.dialog.latencyText(2) == "Normal (10,7 ms)");

    RecordingCanvas canvas;
    english.dialog.draw(canvas);
    CHECK(canvas.drew("Devices"));
    CHECK(canvas.drew("Latency"));
    CHECK(canvas.drew("Short (5.3 ms)")); // the one set now
    CHECK(canvas.drew("Audio settings take effect at the next start."));
    CHECK(canvas.drew("No controller connected"));
}

TEST_CASE("DevicesDialog: a latency picked from the list and OK accept it, Cancel leaves it")
{
    DialogFixture fixture;
    CHECK_FALSE(fixture.dialog.takeOutcome().has_value());
    fixture.tap(DialogFixture::middle(fixture.dialog.latencyList().bounds()));
    REQUIRE(fixture.dialog.latencyList().isOpen());
    fixture.tap(DialogFixture::middle(fixture.dialog.latencyList().itemArea(3)));
    CHECK(fixture.dialog.chosen().buffer_frames == 1024);
    fixture.tap(DialogFixture::middle(fixture.dialog.okButton().bounds()));
    CHECK(fixture.dialog.takeOutcome() == DevicesDialog::Outcome::Accepted);

    fixture.tap(DialogFixture::middle(fixture.dialog.cancelButton().bounds()));
    CHECK(fixture.dialog.takeOutcome() == DevicesDialog::Outcome::Cancelled);
}

TEST_CASE("DevicesDialog: Enter accepts, Escape closes an open list first and then cancels")
{
    DialogFixture fixture;
    fixture.dialog.keyboard(EditKey::Enter);
    CHECK(fixture.dialog.takeOutcome() == DevicesDialog::Outcome::Accepted);

    fixture.tap(DialogFixture::middle(fixture.dialog.latencyList().bounds()));
    REQUIRE(fixture.dialog.latencyList().isOpen());
    fixture.dialog.keyboard(EditKey::Escape);
    CHECK_FALSE(fixture.dialog.latencyList().isOpen());
    CHECK_FALSE(fixture.dialog.takeOutcome().has_value());
    fixture.dialog.keyboard(EditKey::Escape);
    CHECK(fixture.dialog.takeOutcome() == DevicesDialog::Outcome::Cancelled);
}

TEST_CASE("DevicesDialog: it is modal, a tap outside the panel reaches nothing below it")
{
    // A button that lies below the dialog in the same window
    TextButton    below{"below"};
    PointerRouter router;
    below.setBounds({10, 10, 100, 40});
    router.add(below);
    Translator    translator;
    DevicesDialog dialog{translator, Settings{}, 48'000};
    dialog.layout({0, 0, 1000, 700});
    dialog.addTo(router);
    router.handle({{1, 1}, PointerPhase::Down, {20, 20}});
    router.handle({{1, 1}, PointerPhase::Up, {20, 20}});
    CHECK_FALSE(below.takePress());
    CHECK(dialog.panel().width > 0);
}

TEST_CASE("DevicesDialog: the output, the channels and each controller's mapping are chosen from lists")
{
    Translator    translator;
    DeviceChoices choices{.outputs       = {"Speakers", "DJControl Starlight Analog Surround 4.0"},
                          .ports         = {"DJControl Starlight MIDI 1", "Pad"},
                          .mappings      = {"Hercules DJControl Starlight", "Pad mapping"},
                          .port_defaults = {"Hercules DJControl Starlight", ""}};
    DevicesDialog dialog{translator, Settings{}, 48'000, choices};
    PointerRouter router;
    dialog.layout({0, 0, 1000, 900});
    dialog.addTo(router);
    dialog.update();
    const auto tap = [&](const Rect &area)
    {
        const Point at{area.x + (area.width / 2), area.y + (area.height / 2)};
        router.handle({{1, 1}, PointerPhase::Down, at});
        router.handle({{1, 1}, PointerPhase::Up, at});
        dialog.update();
    };
    // Automatic and stereo to start with; the ports show what they use without a choice
    CHECK(dialog.outputList().selectedIndex() == 0U);
    CHECK(dialog.channelList().selectedIndex() == 0U);
    CHECK(dialog.mappingList(0).selectedIndex() == 1U); // the Starlight mapping
    CHECK(dialog.mappingList(1).selectedIndex() == 0U); // none

    tap(dialog.outputList().bounds());
    tap(dialog.outputList().itemArea(2));
    tap(dialog.channelList().bounds());
    tap(dialog.channelList().itemArea(1));
    tap(dialog.mappingList(0).bounds());
    tap(dialog.mappingList(0).itemArea(0)); // switched off
    tap(dialog.mappingList(1).bounds());
    tap(dialog.mappingList(1).itemArea(2));
    CHECK(dialog.chosen().output_device == "DJControl Starlight Analog Surround 4.0");
    CHECK(dialog.chosen().headphone_output);
    CHECK(dialog.chosen().controllers.at("DJControl Starlight MIDI 1").empty());
    CHECK(dialog.chosen().controllers.at("Pad") == "Pad mapping");

    // Back to Automatic
    tap(dialog.outputList().bounds());
    tap(dialog.outputList().itemArea(0));
    CHECK(dialog.chosen().output_device.empty());
}

TEST_CASE("DevicesDialog: an output chosen before that is not there now stays in the list")
{
    Translator translator;
    Settings   settings;
    settings.output_device = "Gone Card";
    DevicesDialog dialog{translator, settings, 48'000,
                         DeviceChoices{.outputs = {"Speakers"}, .ports = {}, .mappings = {}, .port_defaults = {}}};
    dialog.layout({0, 0, 1000, 900});
    dialog.update();
    CHECK(dialog.outputList().selectedIndex() == 2U);
    CHECK(dialog.chosen().output_device == "Gone Card");
}
