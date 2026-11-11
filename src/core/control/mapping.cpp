// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "control/mapping.hpp"

#include <toml++/toml.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <sstream>
#include <utility>

namespace djw::core::control
{
    namespace
    {
        constexpr std::array<std::pair<Action, std::string_view>, 14> action_names{{
            {Action::Shift, "shift"},
            {Action::DeckPlay, "deck.play"},
            {Action::DeckCue, "deck.cue"},
            {Action::DeckLoad, "deck.load"},
            {Action::DeckKeylock, "deck.keylock"},
            {Action::DeckHeadphones, "deck.headphones"},
            {Action::DeckBendDown, "deck.bend_down"},
            {Action::DeckBendUp, "deck.bend_up"},
            {Action::DeckPitch, "deck.pitch"},
            {Action::DeckVolume, "deck.volume"},
            {Action::DeckHeadphoneLevel, "deck.headphone_level"},
            {Action::DeckSeek, "deck.seek"},
            {Action::Crossfader, "mixer.crossfader"},
            {Action::LibraryMove, "library.move"},
        }};

        /// The actions that belong to a player and need its number
        bool isDeckAction(Action action) noexcept
        {
            return actionName(action).starts_with("deck.");
        }

        std::string lower(std::string_view text)
        {
            std::string result{text};
            std::ranges::transform(result, result.begin(),
                                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return result;
        }

        std::optional<Mode> modeOf(std::string_view name)
        {
            if (name == "button")
            {
                return Mode::Button;
            }
            if (name == "absolute")
            {
                return Mode::Absolute;
            }
            if (name == "absolute14")
            {
                return Mode::Absolute14;
            }
            if (name == "relative")
            {
                return Mode::Relative;
            }
            return std::nullopt;
        }

        std::string where(const toml::node &node)
        {
            return "line " + std::to_string(node.source().begin.line);
        }

        /// The message of midi = { type, channel, number }
        struct Midi
        {
                MidiKind     kind;
                int          channel;
                std::uint8_t number;
        };

        std::expected<Midi, std::string> midiOf(const toml::table &entry, std::string_view what)
        {
            const toml::table *midi = entry["midi"].as_table();
            if (midi == nullptr)
            {
                return std::unexpected{where(entry) + ": a " + std::string{what} +
                                       " needs midi = { type, channel, number }"};
            }
            const auto type    = (*midi)["type"].value<std::string>();
            const auto channel = (*midi)["channel"].value<std::int64_t>();
            const auto number  = (*midi)["number"].value<std::int64_t>();
            if (!type || (*type != "note" && *type != "cc") || !channel || *channel < 1 || *channel > 16 || !number ||
                *number < 0 || *number > 127)
            {
                return std::unexpected{where(*midi) + ": midi needs type \"note\" or \"cc\", channel 1 to 16 and "
                                                      "number 0 to 127"};
            }
            return Midi{.kind    = *type == "note" ? MidiKind::Note : MidiKind::ControlChange,
                        .channel = static_cast<int>(*channel),
                        .number  = static_cast<std::uint8_t>(*number)};
        }

        std::expected<Light, std::string> lightOfEntry(const toml::table &entry)
        {
            const auto midi = midiOf(entry, "light");
            if (!midi)
            {
                return std::unexpected{midi.error()};
            }
            const auto what   = entry["what"].value<std::string>();
            const auto source = what ? lightOf(*what) : std::nullopt;
            if (!source)
            {
                return std::unexpected{where(entry) + ": unknown light \"" + what.value_or("") + "\""};
            }
            const auto deck = entry["deck"].value<std::int64_t>();
            if (!deck || *deck < 1)
            {
                return std::unexpected{where(entry) + ": a light needs the player, deck = 1 or more"};
            }
            const auto on  = entry["on"].value_or(std::int64_t{0x7F});
            const auto off = entry["off"].value_or(std::int64_t{0x00});
            if (on < 0 || on > 127 || off < 0 || off > 127)
            {
                return std::unexpected{where(entry) + ": on and off are 0 to 127"};
            }
            return Light{.source  = *source,
                         .deck    = static_cast<std::size_t>(*deck - 1),
                         .kind    = midi->kind,
                         .channel = midi->channel,
                         .number  = midi->number,
                         .on      = static_cast<std::uint8_t>(on),
                         .off     = static_cast<std::uint8_t>(off)};
        }

        std::expected<Control, std::string> controlOf(const toml::table &entry)
        {
            Control    control;
            const auto midi = midiOf(entry, "control");
            if (!midi)
            {
                return std::unexpected{midi.error()};
            }
            control.kind    = midi->kind;
            control.channel = midi->channel;
            control.number  = midi->number;

            const auto action_name = entry["action"].value<std::string>();
            const auto action      = action_name ? actionOf(*action_name) : std::nullopt;
            if (!action)
            {
                return std::unexpected{where(entry) + ": unknown action \"" + action_name.value_or("") + "\""};
            }
            control.action = *action;

            if (const auto mode_name = entry["mode"].value<std::string>())
            {
                const auto mode = modeOf(*mode_name);
                if (!mode)
                {
                    return std::unexpected{where(entry) + ": unknown mode \"" + *mode_name + "\""};
                }
                control.mode = *mode;
            }
            if (control.mode == Mode::Absolute14 && (control.kind != MidiKind::ControlChange || control.number >= 32))
            {
                return std::unexpected{where(entry) + ": absolute14 needs a cc below 32 (the low part is number + 32)"};
            }

            const auto deck = entry["deck"].value<std::int64_t>();
            if (isDeckAction(control.action))
            {
                if (!deck || *deck < 1)
                {
                    return std::unexpected{where(entry) + ": \"" + std::string{actionName(control.action)} +
                                           "\" needs the player, deck = 1 or more"};
                }
                control.deck = static_cast<std::size_t>(*deck - 1);
            }
            control.shift  = entry["shift"].value_or(false);
            control.invert = entry["invert"].value_or(false);
            return control;
        }
    } // namespace

    std::string_view lightName(LightSource source) noexcept
    {
        switch (source)
        {
            case LightSource::DeckPlaying:
                return "deck.playing";
            case LightSource::DeckCue:
                return "deck.cue";
            case LightSource::DeckHeadphones:
                return "deck.headphones";
        }
        return {};
    }

    std::optional<LightSource> lightOf(std::string_view name) noexcept
    {
        for (const LightSource source : {LightSource::DeckPlaying, LightSource::DeckCue, LightSource::DeckHeadphones})
        {
            if (lightName(source) == name)
            {
                return source;
            }
        }
        return std::nullopt;
    }

    std::string_view actionName(Action action) noexcept
    {
        const auto found = std::ranges::find(action_names, action, &std::pair<Action, std::string_view>::first);
        return found != action_names.end() ? found->second : std::string_view{};
    }

    std::optional<Action> actionOf(std::string_view name) noexcept
    {
        const auto found = std::ranges::find(action_names, name, &std::pair<Action, std::string_view>::second);
        return found != action_names.end() ? std::optional{found->first} : std::nullopt;
    }

    bool Mapping::matches(std::string_view port) const
    {
        const std::string name = lower(port);
        return std::ranges::any_of(match, [&name](const std::string &part)
                                   { return !part.empty() && name.find(lower(part)) != std::string::npos; });
    }

    std::expected<Mapping, std::string> parseMapping(std::string_view text)
    {
        toml::parse_result parsed = toml::parse(text);
        if (!parsed)
        {
            const toml::parse_error &error = parsed.error();
            return std::unexpected{"line " + std::to_string(error.source().begin.line) + ": " +
                                   std::string{error.description()}};
        }
        const toml::table &root   = parsed.table();
        const toml::table *device = root["device"].as_table();
        if (device == nullptr)
        {
            return std::unexpected{std::string{"a mapping needs a [device] with name and match"}};
        }
        Mapping mapping;
        mapping.name  = (*device)["name"].value_or(std::string{});
        mapping.audio = (*device)["audio"].value_or(std::string{});
        if (const toml::array *match = (*device)["match"].as_array())
        {
            for (const toml::node &part : *match)
            {
                if (const auto text_part = part.value<std::string>())
                {
                    mapping.match.push_back(*text_part);
                }
            }
        }
        if (mapping.name.empty() || mapping.match.empty())
        {
            return std::unexpected{where(*device) + ": [device] needs a name and match = [\"part of the port name\"]"};
        }
        if (const toml::array *controls = root["control"].as_array())
        {
            for (const toml::node &node : *controls)
            {
                const toml::table *entry = node.as_table();
                if (entry == nullptr)
                {
                    return std::unexpected{where(node) + ": [[control]] entries must be tables"};
                }
                auto control = controlOf(*entry);
                if (!control)
                {
                    return std::unexpected{control.error()};
                }
                mapping.controls.push_back(*control);
            }
        }
        if (const toml::array *lights = root["light"].as_array())
        {
            for (const toml::node &node : *lights)
            {
                const toml::table *entry = node.as_table();
                if (entry == nullptr)
                {
                    return std::unexpected{where(node) + ": [[light]] entries must be tables"};
                }
                auto light = lightOfEntry(*entry);
                if (!light)
                {
                    return std::unexpected{light.error()};
                }
                mapping.lights.push_back(*light);
            }
        }
        return mapping;
    }

    std::expected<Mapping, std::string> readMapping(const std::filesystem::path &file)
    {
        const std::ifstream in{file};
        if (!in)
        {
            return std::unexpected{"cannot read " + file.string()};
        }
        std::ostringstream text;
        text << in.rdbuf();
        auto mapping = parseMapping(text.str());
        if (!mapping)
        {
            return std::unexpected{file.string() + ": " + mapping.error()};
        }
        mapping->file = file;
        return mapping;
    }

    std::vector<Mapping> readMappings(const std::vector<std::filesystem::path> &folders,
                                      std::vector<std::string>                 &errors)
    {
        std::vector<Mapping> mappings;
        for (const std::filesystem::path &folder : folders)
        {
            std::error_code                    error;
            std::vector<std::filesystem::path> files;
            for (const auto &entry : std::filesystem::directory_iterator{folder, error})
            {
                if (entry.is_regular_file(error) && entry.path().extension() == ".toml")
                {
                    files.push_back(entry.path());
                }
            }
            std::ranges::sort(files); // the same order every time
            for (const std::filesystem::path &file : files)
            {
                if (auto mapping = readMapping(file))
                {
                    mappings.push_back(std::move(*mapping));
                }
                else
                {
                    errors.push_back(mapping.error());
                }
            }
        }
        return mappings;
    }
} // namespace djw::core::control
