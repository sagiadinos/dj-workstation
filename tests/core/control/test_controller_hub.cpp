// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "control/controller_hub.hpp"

#include "temp_dir.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>

#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace djw::core::control;

namespace
{
    /// MIDI ports the test lists and sends from
    class FakeMidiSystem final : public MidiSystem
    {
        public:
            struct State
            {
                    std::vector<std::string>       ports;
                    std::map<std::string, Receive> open;
                    std::vector<std::string>       failing;
                    std::vector<std::string>       outputs;
                    /// What was sent to each output, in order, also after it closed
                    std::map<std::string, std::vector<MidiMessage>> sent;
            };

            explicit FakeMidiSystem(std::shared_ptr<State> state) : state{std::move(state)} {}

            std::vector<std::string> ports() override { return state->ports; }

            std::expected<std::unique_ptr<MidiPort>, std::string> open(std::string_view port, Receive receive) override
            {
                if (std::ranges::find(state->failing, port) != state->failing.end())
                {
                    return std::unexpected{"cannot open " + std::string{port}};
                }
                state->open[std::string{port}] = std::move(receive);
                return std::make_unique<Port>(state, std::string{port});
            }

            std::vector<std::string> outputPorts() override { return state->outputs; }

            std::expected<std::unique_ptr<MidiOutput>, std::string> openOutput(std::string_view port) override
            {
                return std::make_unique<Output>(state, std::string{port});
            }

        private:
            class Output final : public MidiOutput
            {
                public:
                    Output(std::shared_ptr<State> state, std::string name)
                        : state{std::move(state)}, name{std::move(name)}
                    {
                    }
                    void send(const MidiMessage &message) override { state->sent[name].push_back(message); }

                private:
                    std::shared_ptr<State> state;
                    std::string            name;
            };

            class Port final : public MidiPort
            {
                public:
                    Port(std::shared_ptr<State> state, std::string name)
                        : state{std::move(state)}, name{std::move(name)}
                    {
                    }
                    ~Port() override { state->open.erase(name); }
                    Port(const Port &)            = delete;
                    Port &operator=(const Port &) = delete;
                    Port(Port &&)                 = delete;
                    Port &operator=(Port &&)      = delete;

                private:
                    std::shared_ptr<State> state;
                    std::string            name;
            };

            std::shared_ptr<State> state;
    };

    Mapping mappingFor(std::string name, std::string match)
    {
        Mapping mapping{.name = std::move(name), .match = {std::move(match)}, .audio = {}, .controls = {}, .file = {}};
        mapping.controls = {{.kind    = MidiKind::Note,
                             .channel = 1,
                             .number  = 7,
                             .action  = Action::DeckPlay,
                             .deck    = 0,
                             .mode    = Mode::Button,
                             .shift   = false,
                             .invert  = false}};
        return mapping;
    }

    const MidiMessage play{.status = 0x90, .data1 = 7, .data2 = 127};
} // namespace

TEST_CASE("ControllerHub: a port with a matching mapping is opened, its events come out of poll in order")
{
    auto state   = std::make_shared<FakeMidiSystem::State>();
    state->ports = {"Midi Through Port-0", "DJControl Starlight MIDI 1"};
    ControllerHub hub{std::make_unique<FakeMidiSystem>(state), {mappingFor("Starlight", "starlight")}};
    CHECK(hub.connect().empty());
    REQUIRE(hub.connections().size() == 1);
    CHECK(hub.connections()[0].port == "DJControl Starlight MIDI 1");
    CHECK(hub.connections()[0].mapping == "Starlight");
    CHECK(state->open.size() == 1);

    state->open.at("DJControl Starlight MIDI 1")(play);
    state->open.at("DJControl Starlight MIDI 1")({.status = 0x80, .data1 = 7, .data2 = 0});
    const auto events = hub.poll();
    REQUIRE(events.size() == 2);
    CHECK(events[0].pressed);
    CHECK_FALSE(events[1].pressed);
    CHECK(hub.poll().empty());
}

TEST_CASE("ControllerHub: an assignment picks the mapping of a port or switches it off; errors are returned")
{
    auto state   = std::make_shared<FakeMidiSystem::State>();
    state->ports = {"Pad A", "Pad B", "Pad C"};
    ControllerHub hub{std::make_unique<FakeMidiSystem>(state),
                      {mappingFor("First", "pad"), mappingFor("Second", "nothing")}};
    const auto    errors = hub.connect({{"Pad A", "Second"}, {"Pad B", ""}, {"Pad C", "Missing"}});
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].find("Missing") != std::string::npos);
    REQUIRE(hub.connections().size() == 1);
    CHECK(hub.connections()[0].port == "Pad A");
    CHECK(hub.connections()[0].mapping == "Second");

    // Connecting again closes what was open first
    state->failing   = {"Pad A"};
    const auto again = hub.connect();
    CHECK(again.size() == 1);
    REQUIRE(hub.connections().size() == 2); // B and C with the first mapping that matches
    CHECK_FALSE(state->open.contains("Pad A"));
}

TEST_CASE("ControllerHub: closing the hub closes the ports")
{
    auto state   = std::make_shared<FakeMidiSystem::State>();
    state->ports = {"Pad"};
    {
        ControllerHub hub{std::make_unique<FakeMidiSystem>(state), {mappingFor("Pad", "pad")}};
        static_cast<void>(hub.connect());
        CHECK(state->open.size() == 1);
    }
    CHECK(state->open.empty());
}

TEST_CASE("ControllerHub: ports are known without the numbers ALSA adds, so an assignment survives a replug")
{
    CHECK(ControllerHub::stablePortName("DJControl Starlight:DJControl Starlight MIDI 1 24:0") ==
          "DJControl Starlight:DJControl Starlight MIDI 1");
    CHECK(ControllerHub::stablePortName("Pad 1") == "Pad 1");
    CHECK(ControllerHub::stablePortName("Pad a:b") == "Pad a:b");
    CHECK(ControllerHub::stablePortName("Pad") == "Pad");

    auto state   = std::make_shared<FakeMidiSystem::State>();
    state->ports = {"Pad MIDI 1 28:0"};
    ControllerHub hub{std::make_unique<FakeMidiSystem>(state), {mappingFor("First", "pad"), mappingFor("Second", "x")}};
    static_cast<void>(hub.connect({{"Pad MIDI 1", "Second"}}));
    REQUIRE(hub.connections().size() == 1);
    CHECK(hub.connections()[0].port == "Pad MIDI 1");
    CHECK(hub.connections()[0].mapping == "Second");
}

TEST_CASE("ControllerHub: ALSA's Midi Through and the program's own ports are no controllers: not listed, not opened")
{
    auto state   = std::make_shared<FakeMidiSystem::State>();
    state->ports = {"Midi Through:Midi Through Port-0 14:0", "djw:djw 129:0", "Pad 20:0"};
    ControllerHub hub{std::make_unique<FakeMidiSystem>(state), {mappingFor("Any", "")}};
    CHECK(hub.ports() == std::vector<std::string>{"Pad"});
    static_cast<void>(hub.connect({{"Midi Through:Midi Through Port-0", "Any"}}));
    CHECK_FALSE(state->open.contains("Midi Through:Midi Through Port-0 14:0"));
}

TEST_CASE("ControllerHub: an imported Mixxx mapping is saved in the last folder and can be used at once")
{
    const djw::tests::TempDir folder;
    const auto                mixxx = folder.path() / "Test Pad.midi.xml";
    std::ofstream{mixxx} << R"(<MixxxMIDIPreset><info><name>Test Pad</name></info><controller id="Pad"><controls>
        <control><group>[Channel1]</group><key>play</key><status>0x90</status><midino>0x07</midino>
        <options><normal/></options></control>
        <control><group>[Channel1]</group><key>Pad.jog</key><status>0xB0</status><midino>0x0A</midino>
        <options><script-binding/></options></control>
        </controls></controller></MixxxMIDIPreset>)";
    auto state   = std::make_shared<FakeMidiSystem::State>();
    state->ports = {"Pad 20:0"};
    ControllerHub hub{std::make_unique<FakeMidiSystem>(state), {}, {folder.path() / "own"}};
    CHECK(hub.mappingNames().empty());

    const auto imported = hub.importMixxx(mixxx);
    REQUIRE_MESSAGE(imported.has_value(), (imported ? "" : imported.error()));
    CHECK(imported->name == "Test Pad (Mixxx)");
    CHECK(imported->taken == 1);
    CHECK(imported->scripted == 1);
    CHECK(std::filesystem::exists(folder.path() / "own" / "Test Pad (Mixxx).toml"));
    CHECK(hub.mappingNames() == std::vector<std::string>{"Test Pad (Mixxx)"});
    static_cast<void>(hub.connect());
    REQUIRE(hub.connections().size() == 1);
    CHECK(hub.connections()[0].mapping == "Test Pad (Mixxx)");

    CHECK_FALSE(hub.importMixxx(folder.path() / "missing.midi.xml").has_value());
    ControllerHub no_folder{std::make_unique<FakeMidiSystem>(state), {}};
    CHECK_FALSE(no_folder.importMixxx(mixxx).has_value());
}

TEST_CASE("ControllerHub: lights go to the output of the same name, only when they change, and out at the end")
{
    auto state      = std::make_shared<FakeMidiSystem::State>();
    state->ports    = {"Pad 20:0"};
    state->outputs  = {"Pad 20:1"};
    Mapping mapping = mappingFor("Pad", "pad");
    mapping.lights  = {{.source  = LightSource::DeckPlaying,
                        .deck    = 0,
                        .kind    = MidiKind::Note,
                        .channel = 2,
                        .number  = 7,
                        .on      = 0x7F,
                        .off     = 0x00},
                       {.source  = LightSource::DeckCue,
                        .deck    = 1,
                        .kind    = MidiKind::Note,
                        .channel = 3,
                        .number  = 6,
                        .on      = 0x7F,
                        .off     = 0x00}};
    {
        ControllerHub hub{std::make_unique<FakeMidiSystem>(state), {mapping}};
        CHECK(hub.connect().empty());
        hub.setLights({{.playing = true, .cue = false, .headphones = false}}); // no second player: its light off
        auto &sent = state->sent["Pad 20:1"];
        REQUIRE(sent.size() == 2);
        CHECK(sent[0] == MidiMessage{.status = 0x91, .data1 = 7, .data2 = 0x7F});
        CHECK(sent[1] == MidiMessage{.status = 0x92, .data1 = 6, .data2 = 0x00});
        hub.setLights({{.playing = true, .cue = false, .headphones = false}});
        CHECK(sent.size() == 2); // nothing changed, nothing sent
        hub.setLights({{.playing = false, .cue = false, .headphones = false},
                       {.playing = false, .cue = true, .headphones = false}});
        REQUIRE(sent.size() == 4);
        CHECK(sent[2].data2 == 0x00);
        CHECK(sent[3] == MidiMessage{.status = 0x92, .data1 = 6, .data2 = 0x7F});
    }
    // Closed: every light off
    const auto &sent = state->sent["Pad 20:1"];
    REQUIRE(sent.size() == 6);
    CHECK(sent[4].data2 == 0x00);
    CHECK(sent[5].data2 == 0x00);
}

TEST_CASE("ControllerHub: a mapping without lights opens no output")
{
    auto state     = std::make_shared<FakeMidiSystem::State>();
    state->ports   = {"Pad 20:0"};
    state->outputs = {"Pad 20:0"};
    ControllerHub hub{std::make_unique<FakeMidiSystem>(state), {mappingFor("Pad", "pad")}};
    static_cast<void>(hub.connect());
    hub.setLights({{.playing = true, .cue = true, .headphones = true}});
    CHECK(state->sent.empty());
}
