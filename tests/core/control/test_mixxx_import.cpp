// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "control/mixxx_import.hpp"

#include <doctest/doctest.h>

#include <string>

using namespace djw::core::control;

namespace
{
    /// A small Mixxx mapping in the style of the real ones, comments between the elements included
    constexpr std::string_view mixxx = R"(<?xml version="1.0" encoding="UTF-8"?>
<MixxxMIDIPreset mixxxVersion="2.2" schemaVersion="1">
    <info>
        <name>Test Pad</name>
    </info>
    <controller id="Test Pad MIDI">
        <scriptfiles><file filename="test.js" functionprefix="Pad"/></scriptfiles>
        <controls>
        // Deck A
            <control>
                <group>[Channel1]</group>
                <key>play</key>
                <status>0x91</status>
                <midino>0x07</midino>
                <options><normal/></options>
            </control>
            <control>
                <group>[Channel2]</group>
                <key>LoadSelectedTrack</key>
                <status>0x92</status>
                <midino>0x10</midino>
                <options><normal/></options>
            </control>
            /* the pitch: high and low part */
            <control>
                <group>[Channel1]</group>
                <key>rate</key>
                <status>0xB1</status>
                <midino>0x08</midino>
                <options><fourteen-bit-msb/><invert/></options>
            </control>
            <control>
                <group>[Channel1]</group>
                <key>rate</key>
                <status>0xB1</status>
                <midino>0x28</midino>
                <options><fourteen-bit-lsb/></options>
            </control>
            <control>
                <group>[Master]</group>
                <key>crossfader</key>
                <status>0xB0</status>
                <midino>0</midino>
                <options><normal/></options>
            </control>
            <control>
                <group>[Playlist]</group>
                <key>SelectTrackKnob</key>
                <status>0xB0</status>
                <midino>0x01</midino>
                <options><selectknob/></options>
            </control>
            <control>
                <group>[Channel1]</group>
                <key>Pad.wheel</key>
                <status>0xB1</status>
                <midino>0x0A</midino>
                <options><script-binding/></options>
            </control>
            <control>
                <group>[Channel1]</group>
                <key>hotcue_1_activate</key>
                <status>0x96</status>
                <midino>0x00</midino>
                <options><normal/></options>
            </control>
            <control>
                <group>[Channel1]</group>
                <key>play</key>
                <status>0x91</status>
                <midino>0x07</midino>
                <options><normal/></options>
            </control>
        </controls>
        <outputs>
            <output>
                <group>[Channel1]</group>
                <key>play_indicator</key>
                <status>0x91</status>
                <midino>0x07</midino>
                <on>0x7f</on>
                <off>0x0</off>
            </output>
            <output>
                <group>[Channel2]</group>
                <key>cue_indicator</key>
                <status>0x92</status>
                <midino>0x06</midino>
                <on>0x40</on>
                <off>0x01</off>
            </output>
            <output>
                <group>[Channel1]</group>
                <key>sync_enabled</key>
                <status>0x91</status>
                <midino>0x05</midino>
            </output>
        </outputs>
    </controller>
</MixxxMIDIPreset>
)";
} // namespace

TEST_CASE("Mixxx import: direct bindings become our controls, the script's and unknown ones are counted")
{
    const auto result = importMixxx(mixxx);
    REQUIRE_MESSAGE(result.has_value(), (result ? "" : result.error()));
    CHECK(result->mapping.name == "Test Pad (Mixxx)");
    CHECK(result->mapping.match == std::vector<std::string>{"Test Pad MIDI"});
    CHECK(result->taken == 5);
    CHECK(result->scripted == 1);
    CHECK(result->unknown == 1); // the hot cue; the low pitch half and the second play binding are no extra count

    const auto &controls = result->mapping.controls;
    REQUIRE(controls.size() == 5);
    CHECK(controls[0].action == Action::DeckPlay);
    CHECK(controls[0].kind == MidiKind::Note);
    CHECK(controls[0].channel == 2);
    CHECK(controls[0].number == 7);
    CHECK(controls[0].deck == 0U);
    CHECK(controls[1].action == Action::DeckLoad);
    CHECK(controls[1].deck == 1U);
    CHECK(controls[2].action == Action::DeckPitch);
    CHECK(controls[2].mode == Mode::Absolute14);
    CHECK(controls[2].invert);
    CHECK(controls[3].action == Action::Crossfader);
    CHECK(controls[3].mode == Mode::Absolute);
    CHECK_FALSE(controls[3].deck.has_value());
    CHECK(controls[4].action == Action::LibraryMove);
    CHECK(controls[4].mode == Mode::Relative);
}

TEST_CASE("Mixxx import: the result written as TOML reads back the same")
{
    const auto result = importMixxx(mixxx);
    REQUIRE(result.has_value());
    const auto again = parseMapping(formatMapping(result->mapping));
    REQUIRE_MESSAGE(again.has_value(), (again ? "" : again.error()));
    CHECK(again->name == result->mapping.name);
    CHECK(again->match == result->mapping.match);
    CHECK(again->controls == result->mapping.controls);
    CHECK(again->lights == result->mapping.lights);
}

TEST_CASE("Mixxx import: what is not a Mixxx mapping, or names no controller, is refused")
{
    CHECK_FALSE(importMixxx("<html></html>").has_value());
    CHECK_FALSE(
        importMixxx("<MixxxMIDIPreset><controller><controls></controls></controller></MixxxMIDIPreset>").has_value());
}

TEST_CASE("Mixxx import: the outputs for play, cue and headphones become lights, others are left out")
{
    const auto result = importMixxx(mixxx);
    REQUIRE(result.has_value());
    REQUIRE(result->mapping.lights.size() == 2);
    CHECK(result->mapping.lights[0] == Light{.source  = LightSource::DeckPlaying,
                                             .deck    = 0,
                                             .kind    = MidiKind::Note,
                                             .channel = 2,
                                             .number  = 7,
                                             .on      = 0x7F,
                                             .off     = 0x00});
    CHECK(result->mapping.lights[1].source == LightSource::DeckCue);
    CHECK(result->mapping.lights[1].deck == 1U);
    CHECK(result->mapping.lights[1].on == 0x40);
    CHECK(result->mapping.lights[1].off == 0x01);
}
