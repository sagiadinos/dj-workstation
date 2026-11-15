// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "devices_dialog.hpp"
#include "pointer_router.hpp"
#include "recording_canvas.hpp"
#include "settings.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <map>
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

TEST_CASE("DevicesDialog: the import button asks the owner to import, a message shows above the buttons")
{
    Translator    translator;
    DevicesDialog dialog{translator, Settings{}, 48'000,
                         DeviceChoices{.outputs       = {},
                                       .ports         = {},
                                       .mappings      = {},
                                       .port_defaults = {},
                                       .message       = "Pad (Mixxx): 7 controls taken over, 23 left out"}};
    PointerRouter router;
    dialog.layout({0, 0, 1000, 900});
    dialog.addTo(router);
    dialog.update();
    RecordingCanvas canvas;
    dialog.draw(canvas);
    CHECK(canvas.drew("Import Mixxx mapping…"));
    CHECK(canvas.drew("Pad (Mixxx): 7 controls taken over, 23 left out"));
    const Rect button = dialog.importButton().bounds();
    REQUIRE(button.width > 0);
    CHECK(button.y + button.height <= dialog.panel().y + dialog.panel().height);
    router.handle({{1, 1}, PointerPhase::Down, {button.x + 5, button.y + 5}});
    router.handle({{1, 1}, PointerPhase::Up, {button.x + 5, button.y + 5}});
    dialog.update();
    CHECK(dialog.takeOutcome() == DevicesDialog::Outcome::ImportMixxx);
}

TEST_CASE("Settings: the text has stereo as 2, no output line for Automatic, and nothing a line cannot hold")
{
    const std::string plain = formatSettings(Settings{});
    CHECK(plain.find("output-channels 2\n") != std::string::npos);
    CHECK(plain.find("output-device") == std::string::npos);
    CHECK(parseSettings("djw-settings 1\nbuffer-frames 1024\n").buffer_frames == 1024);

    // A device or a controller line with a tab or a line break would not read back: left out
    Settings odd;
    odd.output_device      = "two\nlines";
    odd.controllers        = {{"", "Mapping"}, {"tab\tport", "Mapping"}, {"Port", "tab\tmapping"}, {"P", "M"}};
    const std::string text = formatSettings(odd);
    CHECK(text.find("output-device") == std::string::npos);
    CHECK(text.find("tab") == std::string::npos);
    CHECK(text.find("controller \tMapping") == std::string::npos); // no empty port
    // A port of one letter is fine, a line without a port is skipped
    const Settings read = parseSettings(text + "controller \tNo port\n");
    CHECK(read.controllers == std::map<std::string, std::string>{{"P", "M"}});
}

TEST_CASE("Settings: the language is kept by its code; none or an unknown one means the system's")
{
    CHECK_FALSE(parseSettings(formatSettings(Settings{})).language.has_value());
    Settings settings;
    settings.language      = Language::Greek;
    const std::string text = formatSettings(settings);
    CHECK(text.find("language el\n") != std::string::npos);
    CHECK(parseSettings(text) == settings);
    CHECK_FALSE(parseSettings("djw-settings 1\nlanguage xx\n").language.has_value());
}

TEST_CASE("DevicesDialog: rows from the top, labels left, lists right, OK and Cancel at the bottom right, centered")
{
    Translator          translator;
    const DeviceChoices choices{.outputs       = {"Speakers"},
                                .ports         = {"One", "Two", "Three", "Four", "Five"}, // more than max_ports
                                .mappings      = {"Map"},
                                .port_defaults = {"Map"}}; // fewer than ports: the others use none
    DevicesDialog       dialog{translator, Settings{.headphone_output = true}, 48'000, choices};
    dialog.layout({0, 0, 1000, 900});
    dialog.update();

    // Title, four audio rows, two rows of hints, the controllers' heading, four ports, import, message, buttons
    constexpr float padding = 20.0F;
    constexpr float row     = 44.0F;
    constexpr float gap     = 6.0F;
    constexpr float rows    = 15.0F;
    const Rect      panel   = dialog.panel();
    CHECK(panel.height == doctest::Approx((2 * padding) + (rows * row) + ((rows - 1) * gap)));
    CHECK(dialog.panelHeight() == doctest::Approx(panel.height));
    CHECK(panel.width == doctest::Approx(DevicesDialog::panel_width));
    CHECK(panel.x == doctest::Approx((1000 - panel.width) / 2));
    CHECK(panel.y == doctest::Approx((900 - panel.height) / 2));

    const float inner_x = panel.x + padding;
    const float inner_w = panel.width - (2 * padding);
    const float label   = inner_w * 0.3F;
    const float top     = panel.y + padding;
    CHECK(dialog.outputList().bounds().x == doctest::Approx(inner_x + label));
    CHECK(dialog.outputList().bounds().width == doctest::Approx(inner_w - label));
    CHECK(dialog.outputList().bounds().y == doctest::Approx(top + row + gap));
    CHECK(dialog.channelList().bounds().y == doctest::Approx(top + (2 * (row + gap))));
    CHECK(dialog.latencyList().bounds().y == doctest::Approx(top + (3 * (row + gap))));
    CHECK(dialog.keylockList().bounds().y == doctest::Approx(top + (4 * (row + gap))));
    CHECK(dialog.keylockList().bounds().x == doctest::Approx(inner_x + label));
    // After the two rows of hints and the heading: the ports, then the import button
    const float ports_top = top + (5 * (row + gap)) + (2 * row) + gap + row + gap;
    CHECK(dialog.mappingList(0).bounds().y == doctest::Approx(ports_top));
    CHECK(dialog.mappingList(3).bounds().y == doctest::Approx(ports_top + (3 * (row + gap))));
    CHECK(dialog.mappingList(3).bounds().x == doctest::Approx(inner_x + label));
    CHECK(dialog.mappingList(3).bounds().width == doctest::Approx(inner_w - label));
    CHECK_THROWS(static_cast<void>(dialog.mappingList(4))); // the fifth port is left out
    CHECK(dialog.importButton().bounds().y == doctest::Approx(ports_top + (4 * (row + gap))));
    CHECK(dialog.importButton().bounds().x == doctest::Approx(inner_x + label));
    CHECK(dialog.importButton().bounds().width == doctest::Approx(inner_w - label));

    const Rect cancel = dialog.cancelButton().bounds();
    const Rect ok     = dialog.okButton().bounds();
    CHECK(cancel.x + cancel.width == doctest::Approx(inner_x + inner_w));
    CHECK(cancel.y + cancel.height == doctest::Approx(panel.y + panel.height - padding));
    CHECK(ok.y == doctest::Approx(cancel.y));
    CHECK(ok.x + ok.width + 10.0F == doctest::Approx(cancel.x));
    CHECK(dialog.okButton().label() == "OK");
    CHECK(dialog.cancelButton().label() == "Cancel");

    // What is set now; the ports without a default use none
    CHECK(dialog.channelList().selectedIndex() == 1U);
    CHECK(dialog.mappingList(0).selectedIndex() == 1U);
    CHECK(dialog.mappingList(1).selectedIndex() == 0U);

    // The window dimmed first, then everything of the panel
    RecordingCanvas canvas;
    dialog.draw(canvas);
    REQUIRE_FALSE(canvas.rects.empty());
    CHECK(canvas.rects.front().width == doctest::Approx(1000));
    CHECK(canvas.rect_colors.front().alpha == 140);
    CHECK(std::ranges::any_of(canvas.rounded, [&panel](const auto &drawn)
                              { return drawn.rect.x == panel.x && drawn.rect.width == panel.width; }));
    for (const char *text : {"Output", "Channels", "Shorter reacts faster, longer guards against dropouts.",
                             "Controllers", "One", "Four", "OK", "Cancel", "Automatic", "Map"})
    {
        CAPTURE(text);
        CHECK(canvas.drew(text));
    }
    CHECK_FALSE(canvas.drew("Five"));
    // The labels sit in their rows, between the row's top and bottom
    const auto output_text =
        std::ranges::find_if(canvas.texts, [](const auto &drawn) { return drawn.text == "Output"; });
    REQUIRE(output_text != canvas.texts.end());
    CHECK(output_text->position.x == doctest::Approx(inner_x));
    CHECK(output_text->position.y > dialog.outputList().bounds().y);
    CHECK(output_text->position.y < dialog.outputList().bounds().y + (row / 2));
}

TEST_CASE("DevicesDialog: Automatic adds no empty output; Escape closes an open mapping list first")
{
    Translator          translator;
    const DeviceChoices choices{.outputs = {"Speakers"}, .ports = {"Pad"}, .mappings = {"Map"}, .port_defaults = {""}};
    DevicesDialog       dialog{translator, Settings{}, 48'000, choices};
    PointerRouter       router;
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
    tap(dialog.outputList().bounds());
    REQUIRE(dialog.outputList().isOpen());
    // Automatic and Speakers, nothing else
    CHECK(dialog.outputList().listArea().height == doctest::Approx(2 * dialog.outputList().itemArea(0).height));
    dialog.keyboard(EditKey::Escape);

    tap(dialog.mappingList(0).bounds());
    REQUIRE(dialog.mappingList(0).isOpen());
    dialog.keyboard(EditKey::Escape);
    CHECK_FALSE(dialog.mappingList(0).isOpen());
    CHECK_FALSE(dialog.takeOutcome().has_value());

    // A sample rate of 0 counts as 1, so the milliseconds stay finite
    DevicesDialog no_rate{translator, Settings{}, 0};
    CHECK(no_rate.latencyText(0) == "Shortest (128000.0 ms)");
}

TEST_CASE("Settings: the keylock library comes back as saved; none or an unknown one means SoundTouch")
{
    using djw::core::api::TimeStretch;
    CHECK(parseSettings(formatSettings(Settings{})).time_stretch == TimeStretch::SoundTouch);
    CHECK(formatSettings(Settings{}).find("keylock soundtouch\n") != std::string::npos);
    Settings settings;
    settings.time_stretch  = TimeStretch::Signalsmith;
    const std::string text = formatSettings(settings);
    CHECK(text.find("keylock signalsmith\n") != std::string::npos);
    CHECK(parseSettings(text) == settings);
    CHECK(parseSettings("djw-settings 1\nkeylock rubberband\n").time_stretch == TimeStretch::SoundTouch);
    CHECK(parseSettings("djw-settings 1\nkeylock \n").time_stretch == TimeStretch::SoundTouch);
}

TEST_CASE("DevicesDialog: the keylock library is chosen from the ones built in, by their own names")
{
    using djw::core::api::TimeStretch;
    Translator          translator;
    const DeviceChoices choices{.outputs       = {},
                                .ports         = {},
                                .mappings      = {},
                                .port_defaults = {},
                                .message       = {},
                                .stretchers    = {TimeStretch::SoundTouch, TimeStretch::Signalsmith}};
    DevicesDialog       dialog{translator, Settings{.time_stretch = TimeStretch::Signalsmith}, 48'000, choices};
    PointerRouter       router;
    dialog.layout({0, 0, 1000, 900});
    dialog.addTo(router);
    dialog.update();
    CHECK(dialog.keylockList().selectedIndex() == 1U);
    RecordingCanvas canvas;
    dialog.draw(canvas);
    CHECK(canvas.drew("Keylock"));
    CHECK(canvas.drew("Signalsmith Stretch"));

    const auto tap = [&](Point position)
    {
        router.handle({{1, 1}, PointerPhase::Down, position});
        router.handle({{1, 1}, PointerPhase::Up, position});
        dialog.update();
    };
    tap(DialogFixture::middle(dialog.keylockList().bounds()));
    REQUIRE(dialog.keylockList().isOpen());
    tap(DialogFixture::middle(dialog.keylockList().itemArea(0)));
    CHECK(dialog.chosen().time_stretch == TimeStretch::SoundTouch);
    RecordingCanvas after;
    dialog.draw(after);
    CHECK(after.drew("SoundTouch"));

    // A library not built in (a setting from another build): nothing chosen in the list, the setting kept
    DevicesDialog only_soundtouch{translator, Settings{.time_stretch = TimeStretch::Signalsmith}, 48'000};
    only_soundtouch.layout({0, 0, 1000, 900});
    only_soundtouch.update();
    CHECK_FALSE(only_soundtouch.keylockList().selectedIndex().has_value());
    CHECK(only_soundtouch.chosen().time_stretch == TimeStretch::Signalsmith);
}
