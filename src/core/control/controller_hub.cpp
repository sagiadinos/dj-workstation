// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "control/controller_hub.hpp"

#include "control/mixxx_import.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <utility>

namespace djw::core::control
{
    ControllerHub::ControllerHub(std::unique_ptr<MidiSystem> midi, std::vector<Mapping> mappings,
                                 std::vector<std::filesystem::path> folders)
        : midi{std::move(midi)}, all{std::move(mappings)}, folders{std::move(folders)}
    {
    }

    std::expected<api::Controllers::Imported, std::string> ControllerHub::importMixxx(const std::filesystem::path &file)
    {
        if (folders.empty())
        {
            return std::unexpected{std::string{"no folder to keep mappings in"}};
        }
        const std::ifstream in{file};
        if (!in)
        {
            return std::unexpected{"cannot read " + file.string()};
        }
        std::ostringstream text;
        text << in.rdbuf();
        auto imported = control::importMixxx(text.str());
        if (!imported)
        {
            return std::unexpected{file.filename().string() + ": " + imported.error()};
        }
        // Saved under the Mixxx file's name, so importing it again replaces it
        std::error_code error;
        std::filesystem::create_directories(folders.back(), error);
        std::string stem = file.filename().string();
        if (const std::size_t at = stem.find(".midi.xml"); at != std::string::npos)
        {
            stem.resize(at);
        }
        else
        {
            stem = file.stem().string();
        }
        const std::filesystem::path target = folders.back() / (stem + " (Mixxx).toml");
        std::ofstream               out{target, std::ios::trunc};
        out << formatMapping(imported->mapping);
        if (!out)
        {
            return std::unexpected{"cannot write " + target.string()};
        }
        out.close();
        std::vector<std::string> errors;
        all = readMappings(folders, errors);
        return Imported{.name     = imported->mapping.name,
                        .taken    = imported->taken,
                        .scripted = imported->scripted,
                        .unknown  = imported->unknown};
    }

    ControllerHub::~ControllerHub()
    {
        // mull-ignore-next: cxx_remove_void_call
        closeAll();
    }

    namespace
    {
        /// The message that sets a light to value
        MidiMessage messageOf(const Light &light, std::uint8_t value)
        {
            const auto kind = light.kind == MidiKind::Note ? 0x90U : 0xB0U;
            return {.status = static_cast<std::uint8_t>(kind | static_cast<unsigned int>(light.channel - 1)),
                    .data1  = light.number,
                    .data2  = value};
        }
    } // namespace

    void ControllerHub::closeAll()
    {
        // The inputs first: after that no callback can reach a controller any more. The lights go out.
        for (const auto &open : open_ports)
        {
            open->input.reset();
            if (open->output)
            {
                for (const Light &light : open->controller.mapping().lights)
                {
                    open->output->send(messageOf(light, light.off));
                }
            }
        }
        open_ports.clear();
    }

    void ControllerHub::setLights(const std::vector<api::DeckLights> &decks)
    {
        for (const auto &open : open_ports)
        {
            if (!open->output)
            {
                continue;
            }
            const std::vector<Light> &lights = open->controller.mapping().lights;
            open->sent.resize(lights.size());
            for (std::size_t index = 0; index < lights.size(); ++index)
            {
                const Light          &light = lights[index];
                const api::DeckLights state = light.deck < decks.size() ? decks[light.deck] : api::DeckLights{};
                const bool            lit   = light.source == LightSource::DeckPlaying ? state.playing
                                              : light.source == LightSource::DeckCue   ? state.cue
                                                                                       : state.headphones;
                const std::uint8_t    value = lit ? light.on : light.off;
                if (open->sent[index] != value)
                {
                    open->output->send(messageOf(light, value));
                    open->sent[index] = value;
                }
            }
        }
    }

    std::string ControllerHub::stablePortName(std::string_view port)
    {
        // The last word, if it is digits:digits
        const std::size_t space = port.rfind(' ');
        if (space == std::string_view::npos)
        {
            return std::string{port};
        }
        const std::string_view last   = port.substr(space + 1);
        const std::size_t      colon  = last.find(':');
        const auto             digits = [](std::string_view text)
        { return !text.empty() && std::ranges::all_of(text, [](char c) { return c >= '0' && c <= '9'; }); };
        if (colon == std::string_view::npos || !digits(last.substr(0, colon)) || !digits(last.substr(colon + 1)))
        {
            return std::string{port};
        }
        return std::string{port.substr(0, space)};
    }

    bool ControllerHub::isSystemPort(std::string_view port)
    {
        // ALSA's pass-through, and the ports of the program itself (this one or another one running)
        const std::string own = std::string{midi_client_name} + ":";
        return port.starts_with("Midi Through") || port.starts_with(own);
    }

    std::vector<std::string> ControllerHub::ports() const
    {
        std::vector<std::string> names;
        for (const std::string &port : midi->ports())
        {
            if (!isSystemPort(port))
            {
                // mull-ignore-next: cxx_remove_void_call
                names.push_back(stablePortName(port));
            }
        }
        return names;
    }

    std::vector<std::string> ControllerHub::mappingNames() const
    {
        std::vector<std::string> names;
        names.reserve(all.size());
        for (const Mapping &mapping : all)
        {
            names.push_back(mapping.name);
        }
        return names;
    }

    std::string ControllerHub::mappingFor(const std::string &port) const
    {
        const Mapping *mapping = mappingMatching(port);
        return mapping != nullptr ? mapping->name : std::string{};
    }

    std::string ControllerHub::controllerAudio() const
    {
        for (const auto &open : open_ports)
        {
            if (!open->controller.mapping().audio.empty())
            {
                return open->controller.mapping().audio;
            }
        }
        return {};
    }

    const Mapping *ControllerHub::mappingMatching(const std::string &port) const
    {
        const auto found = std::ranges::find_if(all, [&port](const Mapping &mapping) { return mapping.matches(port); });
        return found != all.end() ? &*found : nullptr;
    }

    const Mapping *ControllerHub::mappingNamed(const std::string &name) const
    {
        const auto found = std::ranges::find(all, name, &Mapping::name);
        return found != all.end() ? &*found : nullptr;
    }

    std::vector<std::string> ControllerHub::connect(const Assignments &assigned)
    {
        closeAll();
        std::vector<std::string> errors;
        for (const std::string &port : midi->ports())
        {
            if (isSystemPort(port))
            {
                continue;
            }
            const Mapping    *mapping = nullptr;
            const std::string stable  = stablePortName(port);
            if (const auto chosen = assigned.find(stable); chosen != assigned.end())
            {
                if (chosen->second.empty())
                {
                    continue; // switched off for this port
                }
                mapping = mappingNamed(chosen->second);
                if (mapping == nullptr)
                {
                    // mull-ignore-next: cxx_remove_void_call
                    errors.push_back("no mapping \"" + chosen->second + "\" for MIDI port " + port);
                    continue;
                }
            }
            else
            {
                mapping = mappingMatching(port);
            }
            if (mapping == nullptr)
            {
                continue;
            }
            auto  added = std::make_unique<Open>(Open{.port = stable, .controller = Controller{*mapping}, .input = {}});
            Open &where = *added;
            auto  input = midi->open(port, [this, &where](const MidiMessage &message) { received(where, message); });
            if (!input)
            {
                errors.push_back(input.error());
                continue;
            }
            where.input = std::move(*input);
            // The output of the same name, for the lights
            if (!mapping->lights.empty())
            {
                for (const std::string &output : midi->outputPorts())
                {
                    if (stablePortName(output) == stable)
                    {
                        if (auto opened = midi->openOutput(output))
                        {
                            where.output = std::move(*opened);
                        }
                        else
                        {
                            errors.push_back(opened.error());
                        }
                        break;
                    }
                }
            }
            open_ports.push_back(std::move(added));
        }
        return errors;
    }

    void ControllerHub::received(Open &open, const MidiMessage &message)
    {
        const std::scoped_lock guard{lock};
        for (const ControlEvent &event : open.controller.handle(message))
        {
            queued.push_back(event);
        }
    }

    std::vector<ControlEvent> ControllerHub::poll()
    {
        const std::scoped_lock guard{lock};
        return std::exchange(queued, {});
    }

    std::vector<api::Controllers::Connection> ControllerHub::connections() const
    {
        std::vector<Connection> result;
        result.reserve(open_ports.size());
        for (const auto &open : open_ports)
        {
            // mull-ignore-next: cxx_remove_void_call
            result.push_back({.port = open->port, .mapping = open->controller.mapping().name});
        }
        return result;
    }
} // namespace djw::core::control

namespace djw::core::api
{
    std::unique_ptr<Controllers> openControllers(const std::vector<std::filesystem::path> &mapping_folders,
                                                 std::vector<std::string>                 &errors)
    {
        return std::make_unique<control::ControllerHub>(
            control::createMidiSystem(), control::readMappings(mapping_folders, errors), mapping_folders);
    }
} // namespace djw::core::api
