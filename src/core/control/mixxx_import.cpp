// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "control/mixxx_import.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <format>
#include <optional>
#include <sstream>

namespace djw::core::control
{
    namespace
    {
        /// The text between <tag> and </tag> after from, std::nullopt if there is none
        std::optional<std::string_view> element(std::string_view text, std::string_view tag)
        {
            const std::string open  = "<" + std::string{tag} + ">";
            const std::string close = "</" + std::string{tag} + ">";
            const std::size_t start = text.find(open);
            if (start == std::string_view::npos)
            {
                return std::nullopt;
            }
            const std::size_t end = text.find(close, start + open.size());
            if (end == std::string_view::npos)
            {
                return std::nullopt;
            }
            return text.substr(start + open.size(), end - start - open.size());
        }

        std::string_view trim(std::string_view text)
        {
            const std::size_t first = text.find_first_not_of(" \t\r\n");
            if (first == std::string_view::npos)
            {
                return {};
            }
            return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
        }

        /// A number written as 0x.. or decimal
        std::optional<int> numberOf(std::string_view text)
        {
            text     = trim(text);
            int base = 10;
            if (text.starts_with("0x") || text.starts_with("0X"))
            {
                text = text.substr(2);
                base = 16;
            }
            int value               = 0;
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, base);
            if (error != std::errc{} || end != text.data() + text.size())
            {
                return std::nullopt;
            }
            return value;
        }

        /// A Mixxx key and the action it is in djw
        struct Known
        {
                std::string_view group; ///< "[Channel]" for any player
                std::string_view key;
                Action           action;
        };
        constexpr std::array<Known, 14> known{{
            {"[Channel]", "play", Action::DeckPlay},
            {"[Channel]", "cue_default", Action::DeckCue},
            {"[Channel]", "pfl", Action::DeckHeadphones},
            {"[Channel]", "keylock", Action::DeckKeylock},
            {"[Channel]", "LoadSelectedTrack", Action::DeckLoad},
            {"[Channel]", "rate_temp_down", Action::DeckBendDown},
            {"[Channel]", "rate_temp_up", Action::DeckBendUp},
            {"[Channel]", "volume", Action::DeckVolume},
            {"[Channel]", "rate", Action::DeckPitch},
            {"[Master]", "crossfader", Action::Crossfader},
            {"[Mixer]", "crossfader", Action::Crossfader},
            {"[Playlist]", "SelectTrackKnob", Action::LibraryMove},
            {"[Library]", "MoveVertical", Action::LibraryMove},
            {"[Channel]", "jog", Action::DeckSeek},
        }};

        bool isAbsolute(Action action)
        {
            return action == Action::DeckVolume || action == Action::DeckPitch || action == Action::Crossfader;
        }

        bool isRelative(Action action)
        {
            return action == Action::LibraryMove || action == Action::DeckSeek;
        }

        /// The player of a group "[ChannelN]", from 0
        std::optional<std::size_t> deckOf(std::string_view group)
        {
            if (!group.starts_with("[Channel") || !group.ends_with(']'))
            {
                return std::nullopt;
            }
            const auto number = numberOf(group.substr(8, group.size() - 9));
            if (!number || *number < 1)
            {
                return std::nullopt;
            }
            return static_cast<std::size_t>(*number - 1);
        }

        enum class Taken : std::uint8_t
        {
            Yes,
            Scripted,
            Unknown,
            Skipped ///< a part of one taken elsewhere (the low half of a 14-bit control)
        };

        Taken take(std::string_view block, Mapping &mapping)
        {
            const std::string_view options = element(block, "options").value_or(std::string_view{});
            if (options.find("script-binding") != std::string_view::npos)
            {
                return Taken::Scripted;
            }
            if (options.find("fourteen-bit-lsb") != std::string_view::npos)
            {
                return Taken::Skipped;
            }
            const std::string_view group  = trim(element(block, "group").value_or(std::string_view{}));
            const std::string_view key    = trim(element(block, "key").value_or(std::string_view{}));
            const auto             status = numberOf(element(block, "status").value_or(std::string_view{}));
            const auto             number = numberOf(element(block, "midino").value_or(std::string_view{}));
            if (!status || !number || *number < 0 || *number > 127)
            {
                return Taken::Unknown;
            }
            const auto deck = deckOf(group);
            const auto found =
                std::ranges::find_if(known,
                                     [&](const Known &entry)
                                     {
                                         return entry.key == key && (entry.group == group ||
                                                                     (entry.group == "[Channel]" && deck.has_value()));
                                     });
            const auto status_byte = static_cast<unsigned int>(*status);
            const auto kind_bits   = status_byte & 0xF0U;
            if (found == known.end() || (kind_bits != 0x80U && kind_bits != 0x90U && kind_bits != 0xB0U))
            {
                return Taken::Unknown;
            }
            Control control;
            control.kind    = kind_bits == 0xB0U ? MidiKind::ControlChange : MidiKind::Note;
            control.channel = static_cast<int>(status_byte & 0x0FU) + 1;
            control.number  = static_cast<std::uint8_t>(*number);
            control.action  = found->action;
            control.deck    = found->group == "[Channel]" ? deck : std::nullopt;
            control.invert  = options.find("invert") != std::string_view::npos;
            if (isAbsolute(control.action))
            {
                const bool wide = options.find("fourteen-bit-msb") != std::string_view::npos;
                control.mode = wide && control.kind == MidiKind::ControlChange && control.number < 32 ? Mode::Absolute14
                                                                                                      : Mode::Absolute;
            }
            else if (isRelative(control.action))
            {
                control.mode = Mode::Relative;
            }
            // The first binding of a message counts, as in Mixxx
            const bool taken_before = std::ranges::any_of(mapping.controls,
                                                          [&control](const Control &other)
                                                          {
                                                              return other.kind == control.kind &&
                                                                     other.channel == control.channel &&
                                                                     other.number == control.number;
                                                          });
            if (taken_before)
            {
                return Taken::Skipped;
            }
            mapping.controls.push_back(control);
            return Taken::Yes;
        }

        std::string quoted(std::string_view text)
        {
            std::string result{"\""};
            for (const char c : text)
            {
                if (c == '"' || c == '\\')
                {
                    result += '\\';
                }
                result += c;
            }
            return result + "\"";
        }

        std::string_view modeName(Mode mode)
        {
            switch (mode)
            {
                case Mode::Button:
                    return "button";
                case Mode::Absolute:
                    return "absolute";
                case Mode::Absolute14:
                    return "absolute14";
                case Mode::Relative:
                    return "relative";
            }
            return "button";
        }
    } // namespace

    std::expected<MixxxImport, std::string> importMixxx(std::string_view xml)
    {
        if (xml.find("<MixxxMIDIPreset") == std::string_view::npos &&
            xml.find("<MixxxControllerPreset") == std::string_view::npos)
        {
            return std::unexpected{std::string{"not a Mixxx MIDI mapping"}};
        }
        MixxxImport            result;
        const std::string_view info = element(xml, "info").value_or(std::string_view{});
        const std::string_view name = trim(element(info, "name").value_or(std::string_view{}));
        // The controller's id: <controller id="...">
        std::string id;
        if (const std::size_t at = xml.find("<controller id=\""); at != std::string_view::npos)
        {
            const std::size_t start = at + std::string_view{"<controller id=\""}.size();
            const std::size_t end   = xml.find('"', start);
            if (end != std::string_view::npos)
            {
                id = std::string{xml.substr(start, end - start)};
            }
        }
        result.mapping.name = std::string{name.empty() ? std::string_view{id} : name} + " (Mixxx)";
        if (!id.empty())
        {
            result.mapping.match.push_back(id);
        }
        else if (!name.empty())
        {
            result.mapping.match.emplace_back(name);
        }
        if (result.mapping.match.empty())
        {
            return std::unexpected{std::string{"the Mixxx mapping names no controller"}};
        }

        std::string_view controls = element(xml, "controls").value_or(std::string_view{});
        for (std::size_t at = controls.find("<control>"); at != std::string_view::npos; at = controls.find("<control>"))
        {
            const std::size_t end = controls.find("</control>", at);
            if (end == std::string_view::npos)
            {
                break;
            }
            switch (take(controls.substr(at, end - at), result.mapping))
            {
                case Taken::Yes:
                    ++result.taken;
                    break;
                case Taken::Scripted:
                    ++result.scripted;
                    break;
                case Taken::Unknown:
                    ++result.unknown;
                    break;
                case Taken::Skipped:
                    break;
            }
            controls = controls.substr(end);
        }
        return result;
    }

    std::string formatMapping(const Mapping &mapping)
    {
        std::ostringstream out;
        out << "[device]\n";
        out << "name  = " << quoted(mapping.name) << '\n';
        out << "match = [";
        for (std::size_t index = 0; index < mapping.match.size(); ++index)
        {
            out << (index > 0 ? ", " : "") << quoted(mapping.match.at(index));
        }
        out << "]\n";
        if (!mapping.audio.empty())
        {
            out << "audio = " << quoted(mapping.audio) << '\n';
        }
        for (const Control &control : mapping.controls)
        {
            out << "\n[[control]]\n";
            out << std::format("midi   = {{ type = \"{}\", channel = {}, number = 0x{:02X} }}\n",
                               control.kind == MidiKind::Note ? "note" : "cc", control.channel, control.number);
            out << "action = " << quoted(actionName(control.action)) << '\n';
            if (control.deck)
            {
                out << "deck   = " << (*control.deck + 1) << '\n';
            }
            if (control.mode != Mode::Button)
            {
                out << "mode   = " << quoted(modeName(control.mode)) << '\n';
            }
            if (control.shift)
            {
                out << "shift  = true\n";
            }
            if (control.invert)
            {
                out << "invert = true\n";
            }
        }
        return out.str();
    }
} // namespace djw::core::control
