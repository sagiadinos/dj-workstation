// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "confirm_dialog.hpp"
#include "language_dialog.hpp"
#include "pointer_router.hpp"
#include "recording_canvas.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <string>

using namespace djw::player_ui::sdl;
using djw::tests::RecordingCanvas;

namespace
{
    constexpr Rect window{100, 50, 1000, 700};

    bool sameRect(const Rect &first, const Rect &second)
    {
        return first.x == second.x && first.y == second.y && first.width == second.width &&
               first.height == second.height;
    }

    bool samePoint(Point first, Point second)
    {
        return first.x == second.x && first.y == second.y;
    }

    /// The black over the window below a dialog
    bool darkening(Color color)
    {
        return color.red == 0 && color.green == 0 && color.blue == 0 && color.alpha == 140;
    }

    Point middle(const Rect &area)
    {
        return {area.x + (area.width / 2), area.y + (area.height / 2)};
    }

    void tap(PointerRouter &router, Point position)
    {
        router.handle({{1, 1}, PointerPhase::Down, position});
        router.handle({{1, 1}, PointerPhase::Up, position});
    }

    /// The text drawn as text, or nullptr
    const RecordingCanvas::DrawnText *drawn(const RecordingCanvas &canvas, std::string_view text)
    {
        const auto found = std::ranges::find(canvas.texts, text, &RecordingCanvas::DrawnText::text);
        return found != canvas.texts.end() ? &*found : nullptr;
    }

    struct ConfirmFixture
    {
            explicit ConfirmFixture(Language language = Language::English)
            {
                translator.setLanguage(language);
                below.setBounds({window.x + 5, window.y + 5, 60, 30}); // in a corner, outside the panel
                router.add(below);
                dialog.layout(window);
                dialog.addTo(router);
                dialog.update();
            }

            void tapAt(Point position)
            {
                tap(router, position);
                dialog.update();
            }

            Translator    translator;
            TextButton    below{"below"};
            PointerRouter router;
            ConfirmDialog dialog{translator, "dialog.language.title", "dialog.quit.playing", "dialog.ok"};
    };

    struct LanguageFixture
    {
            LanguageFixture()
            {
                below.setBounds({window.x + 5, window.y + 5, 60, 30});
                router.add(below);
                dialog.layout(window);
                dialog.addTo(router);
                dialog.update();
            }

            void tapAt(Point position)
            {
                tap(router, position);
                dialog.update();
            }

            Translator     translator;
            TextButton     below{"below"};
            PointerRouter  router;
            LanguageDialog dialog{translator, Language::German};
    };

    std::size_t indexOf(Language language)
    {
        return static_cast<std::size_t>(std::ranges::find(languages, language) - languages.begin());
    }
} // namespace

TEST_CASE("ConfirmDialog: a panel in the middle of the window, Cancel at the right, the other button before it")
{
    ConfirmFixture fixture;
    // Title, message and buttons in rows of 44 with gaps of 6, 20 around them
    const Rect panel{window.x + ((window.width - 480) / 2), window.y + ((window.height - 184) / 2), 480, 184};
    CHECK(sameRect(fixture.dialog.panel(), panel));
    CHECK(sameRect(fixture.dialog.cancelButton().bounds(),
                   Rect{panel.x + panel.width - 20 - 160, panel.y + panel.height - 20 - 44, 160, 44}));
    CHECK(sameRect(fixture.dialog.confirmButton().bounds(),
                   Rect{panel.x + panel.width - 20 - 330, panel.y + panel.height - 20 - 44, 160, 44}));

    // A window smaller than the panel: the panel fills it
    fixture.dialog.layout({0, 0, 300, 100});
    CHECK(sameRect(fixture.dialog.panel(), Rect{0, 0, 300, 100}));
}

TEST_CASE("ConfirmDialog: the buttons are named in the user's language, Cancel is the default")
{
    ConfirmFixture english;
    CHECK(english.dialog.confirmButton().label() == "OK");
    CHECK(english.dialog.cancelButton().label() == "Cancel");
    CHECK(english.dialog.cancelButton().isHighlighted());
    CHECK_FALSE(english.dialog.confirmButton().isHighlighted());
    CHECK_FALSE(english.dialog.wantsTextInput());

    ConfirmFixture german{Language::German};
    CHECK(german.dialog.cancelButton().label() == "Abbrechen");
}

TEST_CASE("ConfirmDialog: only a tap on its button confirms; Cancel, Enter and Escape cancel, other keys do nothing")
{
    ConfirmFixture fixture;
    CHECK_FALSE(fixture.dialog.takeOutcome().has_value());

    fixture.tapAt(middle(fixture.dialog.confirmButton().bounds()));
    CHECK(fixture.dialog.takeOutcome() == ConfirmDialog::Outcome::Confirmed);
    CHECK_FALSE(fixture.dialog.takeOutcome().has_value()); // once

    fixture.tapAt(middle(fixture.dialog.cancelButton().bounds()));
    CHECK(fixture.dialog.takeOutcome() == ConfirmDialog::Outcome::Cancelled);

    fixture.dialog.keyboard(EditKey::Enter);
    CHECK(fixture.dialog.takeOutcome() == ConfirmDialog::Outcome::Cancelled);
    fixture.dialog.keyboard(EditKey::Escape);
    CHECK(fixture.dialog.takeOutcome() == ConfirmDialog::Outcome::Cancelled);

    fixture.dialog.keyboard(EditKey::Tab);
    fixture.dialog.keyboard(std::string{"y"});
    fixture.dialog.keyboard(PlayerKey{});
    CHECK_FALSE(fixture.dialog.takeOutcome().has_value());
}

TEST_CASE("ConfirmDialog: it is modal, a tap outside the panel reaches nothing below it and decides nothing")
{
    ConfirmFixture fixture;
    fixture.tapAt(middle(fixture.below.bounds()));
    CHECK_FALSE(fixture.below.takePress());
    CHECK_FALSE(fixture.dialog.takeOutcome().has_value());
}

TEST_CASE("ConfirmDialog: the window darkened, then the panel with title, message and both buttons")
{
    ConfirmFixture  fixture;
    RecordingCanvas canvas;
    fixture.dialog.draw(canvas);
    REQUIRE_FALSE(canvas.rects.empty());
    CHECK(sameRect(canvas.rects.front(), window));
    CHECK(darkening(canvas.rect_colors.front()));
    CHECK_FALSE(canvas.rounded.empty()); // the panel

    const Rect  panel  = fixture.dialog.panel();
    const float middle = (44 - RecordingCanvas::line_height) / 2;
    const auto *title  = drawn(canvas, "Language");
    REQUIRE(title != nullptr);
    CHECK(samePoint(title->position, Point{panel.x + 20, panel.y + 20 + middle}));
    const auto *message = drawn(canvas, "A track is playing. Quit anyway?");
    REQUIRE(message != nullptr);
    CHECK(samePoint(message->position, Point{panel.x + 20, panel.y + 20 + 44 + 6 + middle}));
    CHECK(canvas.drew("OK"));
    CHECK(canvas.drew("Cancel"));
}

TEST_CASE("LanguageDialog: a panel in the middle of the window, the list under the title, OK and Cancel at the bottom")
{
    LanguageFixture fixture;
    // Title, list, a free row and the buttons in rows of 44 with gaps of 6, 20 around them
    const Rect panel{window.x + ((window.width - 420) / 2), window.y + ((window.height - 234) / 2), 420, 234};
    CHECK(sameRect(fixture.dialog.panel(), panel));
    CHECK(sameRect(fixture.dialog.languageList().bounds(), Rect{panel.x + 20, panel.y + 20 + 50, 380, 44}));
    CHECK(sameRect(fixture.dialog.cancelButton().bounds(),
                   Rect{panel.x + panel.width - 20 - 140, panel.y + panel.height - 20 - 44, 140, 44}));
    CHECK(sameRect(fixture.dialog.okButton().bounds(),
                   Rect{panel.x + panel.width - 20 - 290, panel.y + panel.height - 20 - 44, 140, 44}));
    CHECK(fixture.dialog.okButton().label() == "OK");
    CHECK(fixture.dialog.cancelButton().label() == "Cancel");
    CHECK_FALSE(fixture.dialog.wantsTextInput());

    fixture.dialog.layout({0, 0, 300, 100});
    CHECK(sameRect(fixture.dialog.panel(), Rect{0, 0, 300, 100}));
}

TEST_CASE("LanguageDialog: the language in use is chosen at the start; one picked from the list and OK take it")
{
    LanguageFixture fixture;
    CHECK(fixture.dialog.chosen() == Language::German);
    CHECK(fixture.dialog.languageList().selectedIndex() == indexOf(Language::German));

    fixture.tapAt(middle(fixture.dialog.languageList().bounds()));
    REQUIRE(fixture.dialog.languageList().isOpen());
    fixture.tapAt(middle(fixture.dialog.languageList().itemArea(indexOf(Language::French))));
    CHECK(fixture.dialog.chosen() == Language::French);
    CHECK(fixture.dialog.languageList().selectedIndex() == indexOf(Language::French));
    CHECK_FALSE(fixture.dialog.takeOutcome().has_value());

    fixture.tapAt(middle(fixture.dialog.okButton().bounds()));
    CHECK(fixture.dialog.takeOutcome() == LanguageDialog::Outcome::Accepted);
    fixture.tapAt(middle(fixture.dialog.cancelButton().bounds()));
    CHECK(fixture.dialog.takeOutcome() == LanguageDialog::Outcome::Cancelled);
}

TEST_CASE("LanguageDialog: Enter accepts, Escape closes an open list first and then cancels, other keys do nothing")
{
    LanguageFixture fixture;
    fixture.dialog.keyboard(EditKey::Enter);
    CHECK(fixture.dialog.takeOutcome() == LanguageDialog::Outcome::Accepted);

    fixture.dialog.keyboard(EditKey::Tab);
    fixture.dialog.keyboard(std::string{"x"});
    CHECK_FALSE(fixture.dialog.takeOutcome().has_value());

    fixture.tapAt(middle(fixture.dialog.languageList().bounds()));
    REQUIRE(fixture.dialog.languageList().isOpen());
    fixture.dialog.keyboard(EditKey::Escape);
    CHECK_FALSE(fixture.dialog.languageList().isOpen());
    CHECK_FALSE(fixture.dialog.takeOutcome().has_value());
    fixture.dialog.keyboard(EditKey::Escape);
    CHECK(fixture.dialog.takeOutcome() == LanguageDialog::Outcome::Cancelled);
}

TEST_CASE("LanguageDialog: it is modal; it draws the window darkened, the title, the language and both buttons")
{
    LanguageFixture fixture;
    fixture.tapAt(middle(fixture.below.bounds()));
    CHECK_FALSE(fixture.below.takePress());
    CHECK_FALSE(fixture.dialog.takeOutcome().has_value());

    RecordingCanvas canvas;
    fixture.dialog.draw(canvas);
    REQUIRE_FALSE(canvas.rects.empty());
    CHECK(sameRect(canvas.rects.front(), window));
    CHECK(darkening(canvas.rect_colors.front()));
    CHECK_FALSE(canvas.rounded.empty());
    const Rect  panel = fixture.dialog.panel();
    const auto *title = drawn(canvas, "Language");
    REQUIRE(title != nullptr);
    CHECK(samePoint(title->position, Point{panel.x + 20, panel.y + 20 + ((44 - RecordingCanvas::line_height) / 2)}));
    CHECK(canvas.drew("Deutsch"));
    CHECK(canvas.drew("OK"));
    CHECK(canvas.drew("Cancel"));
}
