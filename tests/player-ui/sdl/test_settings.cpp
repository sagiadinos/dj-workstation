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
    CHECK(canvas.drew("Takes effect at the next start."));
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
