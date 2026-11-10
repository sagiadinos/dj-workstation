// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "text_field.hpp"
#include "utf8.hpp"

#include "recording_canvas.hpp"

#include <doctest/doctest.h>

using namespace djw::player_ui::sdl;

TEST_CASE("popCodePoint: removes whole UTF-8 characters")
{
    std::string text = "aé€🎵";
    popCodePoint(text);
    CHECK(text == "aé€");
    popCodePoint(text);
    CHECK(text == "aé");
    popCodePoint(text);
    CHECK(text == "a");
    popCodePoint(text);
    CHECK(text.empty());
    popCodePoint(text);
    CHECK(text.empty());
}

TEST_CASE("TextField: ignores input until a tap focuses it")
{
    TextField field;
    CHECK_FALSE(field.input(std::string{"a"}));
    CHECK(field.text().empty());

    field.pointerDown({0, 0});

    CHECK(field.focused());
    CHECK(field.input(std::string{"a"}));
    CHECK(field.text() == "a");
}

TEST_CASE("TextField: typing appends, Backspace removes a character, Escape clears, Enter changes nothing")
{
    TextField field;
    field.setFocused(true);

    CHECK(field.input(std::string{"Beyonc"}));
    CHECK(field.input(std::string{"é"}));
    CHECK(field.text() == "Beyoncé");
    CHECK(field.input(EditKey::Backspace));
    CHECK(field.text() == "Beyonc");
    CHECK_FALSE(field.input(EditKey::Enter));
    CHECK(field.input(EditKey::Escape));
    CHECK(field.text().empty());
    CHECK_FALSE(field.input(EditKey::Backspace));
}

TEST_CASE("TextField: shows the placeholder while empty, the text otherwise")
{
    TextField field{"Search"};
    field.setBounds({0, 0, 300, 48});
    djw::tests::RecordingCanvas empty;
    field.draw(empty);
    CHECK(empty.drew("Search"));

    field.setFocused(true);
    static_cast<void>(field.input(std::string{"daft"}));
    djw::tests::RecordingCanvas typed;
    field.draw(typed);
    CHECK(typed.drew("daft"));
    CHECK_FALSE(typed.drew("Search"));
}
