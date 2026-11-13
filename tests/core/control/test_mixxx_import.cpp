// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "control/mixxx_import.hpp"

#include <doctest/doctest.h>

#include <optional>
#include <string>
#include <vector>

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

namespace
{
    /// A Mixxx mapping of the controller "Pad" with these controls and outputs, without whitespace between them
    std::string preset(std::string_view controls, std::string_view outputs = {})
    {
        return "<MixxxMIDIPreset><info><name>Pad</name></info><controller id=\"Pad\"><controls>" +
               std::string{controls} + "</controls><outputs>" + std::string{outputs} +
               "</outputs></controller></MixxxMIDIPreset>";
    }

    std::string control(std::string_view group, std::string_view key, std::string_view status, std::string_view midino,
                        std::string_view options = "<normal/>")
    {
        return "<control><group>" + std::string{group} + "</group><key>" + std::string{key} + "</key><status>" +
               std::string{status} + "</status><midino>" + std::string{midino} + "</midino><options>" +
               std::string{options} + "</options></control>";
    }

    /// An output; on or off empty leaves the element out
    std::string output(std::string_view group, std::string_view key, std::string_view status, std::string_view midino,
                       std::string_view on = "0x7F", std::string_view off = "0x00")
    {
        std::string text = "<output><group>" + std::string{group} + "</group><key>" + std::string{key} +
                           "</key><status>" + std::string{status} + "</status><midino>" + std::string{midino} +
                           "</midino>";
        if (!on.empty())
        {
            text += "<on>" + std::string{on} + "</on>";
        }
        if (!off.empty())
        {
            text += "<off>" + std::string{off} + "</off>";
        }
        return text + "</output>";
    }

    /// The import of one control: the control taken, std::nullopt if it was counted as unknown
    std::optional<Control> single(const std::string &control_xml)
    {
        const auto result = importMixxx(preset(control_xml));
        REQUIRE(result.has_value());
        if (result->unknown == 1 && result->mapping.controls.empty())
        {
            return std::nullopt;
        }
        REQUIRE(result->taken == 1);
        REQUIRE(result->mapping.controls.size() == 1);
        return result->mapping.controls.front();
    }

    /// The lights of a mapping with these outputs
    std::vector<Light> lightsOf(const std::string &outputs_xml)
    {
        const auto result = importMixxx(preset({}, outputs_xml));
        REQUIRE(result.has_value());
        return result->mapping.lights;
    }
} // namespace

TEST_CASE("Mixxx import: numbers are hexadecimal with 0x or 0X, otherwise decimal, blanks around them are ignored")
{
    const auto lower = single(control("[Channel1]", "play", "0x91", "0x1f"));
    REQUIRE(lower);
    CHECK(lower->number == 0x1F);
    const auto upper = single(control("[Channel1]", "play", "0X91", "0X1F"));
    REQUIRE(upper);
    CHECK(upper->channel == 2);
    CHECK(upper->number == 0x1F);
    const auto decimal = single(control("[Channel1]", "play", " 145 ", "\n12\t"));
    REQUIRE(decimal);
    CHECK(decimal->channel == 2);
    CHECK(decimal->number == 12);
    CHECK_FALSE(single(control("[Channel1]", "play", "0x91", "12x")));
    CHECK_FALSE(single(control("[Channel1]", "play", "0x91", "")));
}

TEST_CASE("Mixxx import: names and keys are read without the blanks around them")
{
    const auto result =
        importMixxx("<MixxxMIDIPreset><info><name>  Pad Pro \n</name></info><controller id=\"Pad\"><controls>" +
                    control(" [Channel1]\n", "\tplay  ", "0x90", "1") + "</controls></controller></MixxxMIDIPreset>");
    REQUIRE(result.has_value());
    CHECK(result->mapping.name == "Pad Pro (Mixxx)");
    CHECK(result->taken == 1);
}

TEST_CASE("Mixxx import: MIDI numbers from 0 to 127 are taken, the others are unknown")
{
    const auto lowest = single(control("[Channel1]", "play", "0x90", "0"));
    REQUIRE(lowest);
    CHECK(lowest->number == 0);
    const auto highest = single(control("[Channel1]", "play", "0x90", "127"));
    REQUIRE(highest);
    CHECK(highest->number == 127);
    CHECK_FALSE(single(control("[Channel1]", "play", "0x90", "128")));
    CHECK_FALSE(single(control("[Channel1]", "play", "0x90", "-1")));
    CHECK_FALSE(single(control("[Channel1]", "play", "nothing", "1")));
    CHECK_FALSE(single(control("[Channel1]", "play", "0x90", "nothing")));
}

TEST_CASE("Mixxx import: note on, note off and control change are taken, other MIDI messages are unknown")
{
    const auto note_off = single(control("[Channel1]", "play", "0x83", "1"));
    REQUIRE(note_off);
    CHECK(note_off->kind == MidiKind::Note);
    CHECK(note_off->channel == 4);
    const auto note_on = single(control("[Channel1]", "play", "0x9F", "1"));
    REQUIRE(note_on);
    CHECK(note_on->kind == MidiKind::Note);
    CHECK(note_on->channel == 16);
    const auto change = single(control("[Channel1]", "play", "0xB0", "1"));
    REQUIRE(change);
    CHECK(change->kind == MidiKind::ControlChange);
    CHECK(change->channel == 1);
    CHECK_FALSE(single(control("[Channel1]", "play", "0xE0", "1"))); // pitch bend
    CHECK_FALSE(single(control("[Channel1]", "play", "0xA0", "1"))); // aftertouch
}

TEST_CASE("Mixxx import: player controls need a group [ChannelN] with N from 1")
{
    const auto fourth = single(control("[Channel4]", "play", "0x90", "1"));
    REQUIRE(fourth);
    CHECK(fourth->deck == 3U);
    CHECK_FALSE(single(control("[Channel0]", "play", "0x90", "1")));
    CHECK_FALSE(single(control("[ChannelX]", "play", "0x90", "1")));
    CHECK_FALSE(single(control("[Channel12", "play", "0x90", "1")));
    CHECK_FALSE(single(control("[Sampler1]", "play", "0x90", "1")));
    CHECK_FALSE(single(control("[Master]", "play", "0x90", "1")));
}

TEST_CASE("Mixxx import: controls of the mixer and the library need their own group")
{
    CHECK_FALSE(single(control("[Channel1]", "crossfader", "0xB0", "1")));
    CHECK_FALSE(single(control("[Channel1]", "MoveVertical", "0xB0", "1")));
    const auto mixer = single(control("[Mixer]", "crossfader", "0xB0", "1"));
    REQUIRE(mixer);
    CHECK(mixer->action == Action::Crossfader);
    CHECK_FALSE(mixer->deck.has_value());
    const auto library = single(control("[Library]", "MoveVertical", "0xB0", "1"));
    REQUIRE(library);
    CHECK(library->action == Action::LibraryMove);
    CHECK(library->mode == Mode::Relative);
}

TEST_CASE("Mixxx import: buttons, faders and the jog wheel get their mode, invert only when the options say so")
{
    const auto play = single(control("[Channel1]", "play", "0x90", "1"));
    REQUIRE(play);
    CHECK(play->mode == Mode::Button);
    CHECK_FALSE(play->invert);
    const auto jog = single(control("[Channel1]", "jog", "0xB0", "1"));
    REQUIRE(jog);
    CHECK(jog->action == Action::DeckSeek);
    CHECK(jog->mode == Mode::Relative);
    const auto volume = single(control("[Channel1]", "volume", "0xB0", "1"));
    REQUIRE(volume);
    CHECK(volume->mode == Mode::Absolute);
    CHECK_FALSE(volume->invert);
    const auto inverted = single(control("[Channel1]", "volume", "0xB0", "1", "<invert/>"));
    REQUIRE(inverted);
    CHECK(inverted->invert);
}

TEST_CASE("Mixxx import: 14-bit faders need a control change below 32")
{
    const auto wide = single(control("[Channel1]", "volume", "0xB0", "31", "<fourteen-bit-msb/>"));
    REQUIRE(wide);
    CHECK(wide->mode == Mode::Absolute14);
    const auto too_high = single(control("[Channel1]", "volume", "0xB0", "32", "<fourteen-bit-msb/>"));
    REQUIRE(too_high);
    CHECK(too_high->mode == Mode::Absolute);
    const auto note = single(control("[Channel1]", "volume", "0x90", "1", "<fourteen-bit-msb/>"));
    REQUIRE(note);
    CHECK(note->mode == Mode::Absolute);
}

TEST_CASE("Mixxx import: the controller is named after the mapping, or after its id when the mapping has no name")
{
    const std::string controls = "<controls>" + control("[Channel1]", "play", "0x90", "1") + "</controls>";
    const auto        by_id =
        importMixxx("<MixxxMIDIPreset><controller id=\"Pad MIDI 1\">" + controls + "</controller></MixxxMIDIPreset>");
    REQUIRE(by_id.has_value());
    CHECK(by_id->mapping.name == "Pad MIDI 1 (Mixxx)");
    CHECK(by_id->mapping.match == std::vector<std::string>{"Pad MIDI 1"});

    const auto by_name = importMixxx("<MixxxMIDIPreset><info><name>Pad</name></info><controller>" + controls +
                                     "</controller></MixxxMIDIPreset>");
    REQUIRE(by_name.has_value());
    CHECK(by_name->mapping.name == "Pad (Mixxx)");
    CHECK(by_name->mapping.match == std::vector<std::string>{"Pad"});
    CHECK(by_name->taken == 1);
}

TEST_CASE("Mixxx import: the newer root MixxxControllerPreset is read too, other files with a controller are not")
{
    const std::string body =
        "<controller id=\"Pad\"><controls>" + control("[Channel1]", "play", "0x90", "1") + "</controls></controller>";
    const auto newer = importMixxx("<MixxxControllerPreset>" + body + "</MixxxControllerPreset>");
    REQUIRE(newer.has_value());
    CHECK(newer->taken == 1);
    CHECK_FALSE(importMixxx("<SomethingElse>" + body + "</SomethingElse>").has_value());
}

TEST_CASE("Mixxx import: every control of a compact file is read, a control without its end is left out")
{
    const auto result =
        importMixxx(preset(control("[Channel1]", "play", "0x90", "1") + control("[Channel2]", "play", "0x91", "1") +
                           "<control><group>[Channel3]</group><key>play</key>"));
    REQUIRE(result.has_value());
    CHECK(result->taken == 2);
    CHECK(result->unknown == 0);
    REQUIRE(result->mapping.controls.size() == 2);
    CHECK(result->mapping.controls[0].deck == 0U);
    CHECK(result->mapping.controls[1].deck == 1U);
}

TEST_CASE("Mixxx import: the light of headphones is taken, each light of a player only once")
{
    const auto lights =
        lightsOf(output("[Channel1]", "pfl", "0x90", "0x10") + output("[Channel2]", "pfl", "0x91", "0x10") +
                 output("[Channel1]", "pfl", "0x92", "0x20"));
    REQUIRE(lights.size() == 2);
    CHECK(lights[0] == Light{.source  = LightSource::DeckHeadphones,
                             .deck    = 0,
                             .kind    = MidiKind::Note,
                             .channel = 1,
                             .number  = 0x10,
                             .on      = 0x7F,
                             .off     = 0x00});
    CHECK(lights[1].deck == 1U);
    CHECK(lights[1].channel == 2);
}

TEST_CASE("Mixxx import: lights take numbers and values from 0 to 127, without on and off the defaults")
{
    const auto lights = lightsOf(output("[Channel1]", "play_indicator", "0xB0", "0", "0", "127") +
                                 output("[Channel2]", "play_indicator", "0x80", "127", "127", "0") +
                                 output("[Channel3]", "play_indicator", "0x90", "5", "", "") +
                                 output("[Channel4]", "play_indicator", "0x90", "6", "0x40", "0x10"));
    REQUIRE(lights.size() == 4);
    CHECK(lights[0].kind == MidiKind::ControlChange);
    CHECK(lights[0].number == 0);
    CHECK(lights[0].on == 0);
    CHECK(lights[0].off == 127);
    CHECK(lights[1].kind == MidiKind::Note);
    CHECK(lights[1].number == 127);
    CHECK(lights[1].on == 127);
    CHECK(lights[1].off == 0);
    CHECK(lights[2].on == 0x7F); // not the values of the next output
    CHECK(lights[2].off == 0x00);
    CHECK(lights[3].on == 0x40);
    CHECK(lights[3].off == 0x10);
}

TEST_CASE("Mixxx import: an output ends at its own end, also after a long comment before the first one")
{
    const auto lights =
        lightsOf("<!--" + std::string(200, ' ') + "-->" + output("[Channel1]", "play_indicator", "0x90", "5", "", "") +
                 output("[Channel2]", "play_indicator", "0x90", "6", "0x40", "0x10"));
    REQUIRE(lights.size() == 2);
    CHECK(lights[0].on == 0x7F);
    CHECK(lights[0].off == 0x00);
}

TEST_CASE("Mixxx import: outputs with a value out of range, no player or another message are left out")
{
    CHECK(lightsOf(output("[Channel1]", "play_indicator", "0x90", "128")).empty());
    CHECK(lightsOf(output("[Channel1]", "play_indicator", "0x90", "-1")).empty());
    CHECK(lightsOf(output("[Channel1]", "play_indicator", "0x90", "1", "128")).empty());
    CHECK(lightsOf(output("[Channel1]", "play_indicator", "0x90", "1", "-1")).empty());
    CHECK(lightsOf(output("[Channel1]", "play_indicator", "0x90", "1", "0x7F", "128")).empty());
    CHECK(lightsOf(output("[Channel1]", "play_indicator", "0x90", "1", "0x7F", "-1")).empty());
    CHECK(lightsOf(output("[Channel1]", "play_indicator", "0x90", "1", "on")).empty());
    CHECK(lightsOf(output("[Channel1]", "play_indicator", "0x90", "1", "0x7F", "off")).empty());
    CHECK(lightsOf(output("[Channel1]", "play_indicator", "status", "1")).empty());
    CHECK(lightsOf(output("[Channel1]", "play_indicator", "0x90", "number")).empty());
    CHECK(lightsOf(output("[Master]", "play_indicator", "0x90", "1")).empty());
    CHECK(lightsOf(output("[Channel1]", "play_indicator", "0xE0", "1")).empty());
}

TEST_CASE("Mixxx import: the TOML of a mapping has every match, the audio port and a deck only for player controls")
{
    Mapping mapping;
    mapping.name  = "Pad";
    mapping.match = {"Pad A", "Pad B", "Pad C"};
    mapping.audio = "Pad Audio";
    Control crossfader;
    crossfader.kind   = MidiKind::ControlChange;
    crossfader.number = 1;
    crossfader.action = Action::Crossfader;
    crossfader.mode   = Mode::Absolute;
    mapping.controls.push_back(crossfader);
    const std::string text = formatMapping(mapping);
    CHECK(text.find(R"(match = ["Pad A", "Pad B", "Pad C"])") != std::string::npos);
    CHECK(text.find(R"(audio = "Pad Audio")") != std::string::npos);
    CHECK(text.find("deck") == std::string::npos);

    const auto again = parseMapping(text);
    REQUIRE_MESSAGE(again.has_value(), (again ? "" : again.error()));
    CHECK(again->match == mapping.match);
    CHECK(again->audio == mapping.audio);
    CHECK(again->controls == mapping.controls);

    mapping.audio.clear();
    CHECK(formatMapping(mapping).find("audio") == std::string::npos);
}
