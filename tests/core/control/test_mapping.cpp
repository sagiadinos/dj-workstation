// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "control/mapping.hpp"

#include "temp_dir.hpp"

#include <doctest/doctest.h>

#include <fstream>
#include <string>
#include <vector>

using namespace djw::core::control;

namespace
{
    constexpr std::string_view starlight_like = R"(
[device]
name  = "Test Controller"
match = ["DJControl Starlight", "Test Pad"]
audio = "DJControl Starlight"

[[control]]
midi   = { type = "note", channel = 2, number = 0x07 }
action = "deck.play"
deck   = 1

[[control]]
midi   = { type = "cc", channel = 1, number = 0x00 }
action = "mixer.crossfader"
mode   = "absolute"

[[control]]
midi   = { type = "cc", channel = 2, number = 0x08 }
action = "deck.pitch"
deck   = 2
mode   = "absolute14"
shift  = true
invert = true
)";
} // namespace

TEST_CASE("Mapping: device and controls are read from TOML, players count from 1 in the file and from 0 here")
{
    const auto mapping = parseMapping(starlight_like);
    REQUIRE_MESSAGE(mapping.has_value(), (mapping ? "" : mapping.error()));
    CHECK(mapping->name == "Test Controller");
    CHECK(mapping->audio == "DJControl Starlight");
    REQUIRE(mapping->controls.size() == 3);
    CHECK(mapping->controls[0] == Control{.kind    = MidiKind::Note,
                                          .channel = 2,
                                          .number  = 7,
                                          .action  = Action::DeckPlay,
                                          .deck    = 0,
                                          .mode    = Mode::Button,
                                          .shift   = false,
                                          .invert  = false});
    CHECK(mapping->controls[1].action == Action::Crossfader);
    CHECK_FALSE(mapping->controls[1].deck.has_value());
    CHECK(mapping->controls[2].mode == Mode::Absolute14);
    CHECK(mapping->controls[2].deck == 1U);
    CHECK(mapping->controls[2].shift);
    CHECK(mapping->controls[2].invert);
    CHECK_FALSE(mapping->controls[0].invert);

    CHECK(mapping->matches("DJControl Starlight:DJControl Starlight MIDI 1 24:0"));
    CHECK(mapping->matches("my test pad"));
    CHECK_FALSE(mapping->matches("Midi Through Port-0"));
}

TEST_CASE("Mapping: every action has a name and back")
{
    for (const Action action :
         {Action::Shift, Action::DeckPlay, Action::DeckCue, Action::DeckLoad, Action::DeckKeylock,
          Action::DeckHeadphones, Action::DeckBendDown, Action::DeckBendUp, Action::DeckPitch, Action::DeckVolume,
          Action::DeckHeadphoneLevel, Action::DeckSeek, Action::Crossfader, Action::LibraryMove})
    {
        CAPTURE(static_cast<int>(action));
        REQUIRE_FALSE(actionName(action).empty());
        CHECK(actionOf(actionName(action)) == action);
    }
    CHECK_FALSE(actionOf("deck.fly").has_value());
}

TEST_CASE("Mapping: a damaged mapping says what is wrong and where")
{
    const auto error = [](std::string_view text)
    {
        const auto mapping = parseMapping(text);
        REQUIRE_FALSE(mapping.has_value());
        return mapping.error();
    };
    CHECK(error("this is = = not toml").starts_with("line 1"));
    CHECK(error("[[control]]\nmidi = { type = \"note\", channel = 1, number = 1 }\naction = \"deck.play\"\ndeck = 1\n")
              .find("[device]") != std::string::npos);
    const std::string device = "[device]\nname = \"x\"\nmatch = [\"x\"]\n";
    CHECK(error(device + "[[control]]\nmidi = { type = \"note\", channel = 1, number = 1 }\naction = \"deck.fly\"\n"
                         "deck = 1\n")
              .find("unknown action \"deck.fly\"") != std::string::npos);
    CHECK(error(device + "[[control]]\nmidi = { type = \"note\", channel = 1, number = 1 }\naction = \"deck.play\"\n")
              .find("needs the player") != std::string::npos);
    CHECK(error(device + "[[control]]\nmidi = { type = \"note\", channel = 17, number = 1 }\naction = \"shift\"\n")
              .find("line 5") != std::string::npos);
    CHECK(error(device + "[[control]]\nmidi = { type = \"cc\", channel = 1, number = 40 }\naction = \"deck.pitch\"\n"
                         "deck = 1\nmode = \"absolute14\"\n")
              .find("below 32") != std::string::npos);
    CHECK(error(device + "[[control]]\nmidi = { type = \"cc\", channel = 1, number = 1 }\naction = \"shift\"\n"
                         "mode = \"wobbly\"\n")
              .find("unknown mode") != std::string::npos);
    CHECK(error("[device]\nname = \"x\"\n").find("match") != std::string::npos);
}

TEST_CASE("Mapping: all mappings of the folders are read, a broken file is reported and skipped")
{
    const djw::tests::TempDir folder;
    std::ofstream{folder.path() / "b.toml"} << starlight_like;
    std::ofstream{folder.path() / "a.toml"} << "[device]\nname = \"A\"\nmatch = [\"a\"]\n";
    std::ofstream{folder.path() / "broken.toml"} << "[device\n";
    std::ofstream{folder.path() / "notes.txt"} << "not a mapping";
    std::vector<std::string> errors;
    const auto               mappings = readMappings({folder.path(), folder.path() / "missing"}, errors);
    REQUIRE(mappings.size() == 2);
    CHECK(mappings[0].name == "A"); // in file name order
    CHECK(mappings[1].name == "Test Controller");
    CHECK(mappings[1].file == folder.path() / "b.toml");
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].find("broken.toml") != std::string::npos);
}
