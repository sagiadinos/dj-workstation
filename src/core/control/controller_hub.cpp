// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "control/controller_hub.hpp"

#include <algorithm>
#include <utility>

namespace djw::core::control
{
    ControllerHub::ControllerHub(std::unique_ptr<MidiSystem> midi, std::vector<Mapping> mappings)
        : midi{std::move(midi)}, all{std::move(mappings)}
    {
    }

    ControllerHub::~ControllerHub()
    {
        closeAll();
    }

    void ControllerHub::closeAll()
    {
        // The inputs first: after that no callback can reach a controller any more
        for (const auto &open : open_ports)
        {
            open->input.reset();
        }
        open_ports.clear();
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

    const Mapping *ControllerHub::mappingFor(const std::string &port) const
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
                    errors.push_back("no mapping \"" + chosen->second + "\" for MIDI port " + port);
                    continue;
                }
            }
            else
            {
                mapping = mappingFor(port);
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

    std::vector<ControllerHub::Connection> ControllerHub::connections() const
    {
        std::vector<Connection> result;
        result.reserve(open_ports.size());
        for (const auto &open : open_ports)
        {
            result.push_back({.port = open->port, .mapping = open->controller.mapping().name});
        }
        return result;
    }
} // namespace djw::core::control
