// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "control/controller_hub.hpp"

#include <doctest/doctest.h>

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

        private:
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

TEST_CASE("ControllerHub: ALSA's Midi Through port is no controller: not listed, never opened")
{
    auto state   = std::make_shared<FakeMidiSystem::State>();
    state->ports = {"Midi Through:Midi Through Port-0 14:0", "Pad 20:0"};
    ControllerHub hub{std::make_unique<FakeMidiSystem>(state), {mappingFor("Any", "")}};
    CHECK(hub.ports() == std::vector<std::string>{"Pad"});
    static_cast<void>(hub.connect({{"Midi Through:Midi Through Port-0", "Any"}}));
    CHECK_FALSE(state->open.contains("Midi Through:Midi Through Port-0 14:0"));
}
