// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library/text_fold.hpp"

#include <doctest/doctest.h>

using djw::core::library::foldText;

TEST_CASE("foldText: ASCII becomes lower case, the rest stays")
{
    CHECK(foldText("Daft Punk - 1999 (Remix)!") == "daft punk - 1999 (remix)!");
    CHECK(foldText("").empty());
}

TEST_CASE("foldText: Latin letters lose case and accents")
{
    CHECK(foldText("Ärger über Öl") == "arger uber ol");
    CHECK(foldText("Beyoncé") == "beyonce");
    CHECK(foldText("ÇÉÑÅ") == "cena");
    CHECK(foldText("Łódź") == "lodz");
    CHECK(foldText("Dvořák") == "dvorak");
}

TEST_CASE("foldText: letters without a decomposition get their usual Latin spelling")
{
    CHECK(foldText("Straße") == "strasse");
    CHECK(foldText("Ærø") == "aero");
    CHECK(foldText("Œuvre") == "oeuvre");
    CHECK(foldText("Þór") == "thor");
}

TEST_CASE("foldText: other scripts, signs and broken UTF-8 pass through unchanged")
{
    CHECK(foldText("Кино") == "Кино");
    CHECK(foldText("2×3÷4") == "2×3÷4");
    CHECK(foldText("🎵 Ä") == "🎵 a");
    const std::string broken{"a\xC3"};
    CHECK(foldText(broken) == broken);
}

TEST_CASE("foldText: the first and last letter of the folded range are folded, their neighbours outside are not")
{
    CHECK(foldText("À") == "a");         // À, the first in the table
    CHECK(foldText("ſ") == "s");         // ſ, the last
    CHECK(foldText("¿") == "¿");         // ¿, just before the table
    CHECK(foldText("ƀ") == "ƀ");         // ƀ, just after it
    CHECK(foldText("xÀyſz") == "xaysz"); // in the middle of other text
}
