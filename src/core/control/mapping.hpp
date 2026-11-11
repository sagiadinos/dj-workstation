// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_CONTROL_MAPPING_HPP
#define DJW_CORE_CONTROL_MAPPING_HPP

#include "api/controllers.hpp"
#include "control/midi_message.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace djw::core::control
{
    /// What a control does (see api::ControlAction)
    using Action = api::ControlAction;

    /// The name of an action in a mapping file (e.g. "deck.play"), and back
    [[nodiscard]] std::string_view      actionName(Action action) noexcept;
    [[nodiscard]] std::optional<Action> actionOf(std::string_view name) noexcept;

    /// How the value of a control is read
    enum class Mode : std::uint8_t
    {
        Button,     ///< pressed and released (a note on/off, or a control change > 0 / 0)
        Absolute,   ///< 7 bits, 0 to 127
        Absolute14, ///< 14 bits from a control change pair: number is the high part, number + 32 the low part
        Relative,   ///< steps of an encoder: 1 to 63 ahead, 127 down to 65 back (two's complement)
    };

    /// One control of a controller in a mapping
    struct Control
    {
            MidiKind     kind    = MidiKind::Note;
            int          channel = 1; ///< 1 to 16
            std::uint8_t number  = 0;
            Action       action  = Action::DeckPlay;
            /// The player, from 0; std::nullopt for controls that are not a player's
            std::optional<std::size_t> deck;
            Mode                       mode = Mode::Button;
            /// Applies while Shift is held instead of without it
            bool shift = false;
            /// Absolute modes: the value runs the other way (1 - value), e.g. a pitch fader built upside down
            bool invert = false;

            bool operator==(const Control &) const = default;
    };

    /**
     * A controller mapping, read from a TOML file:
     *
     *     [device]
     *     name  = "Hercules DJControl Starlight"
     *     match = ["DJControl Starlight"]   # parts of MIDI port names the mapping is for
     *     audio = "DJControl Starlight"     # optional: part of the name of the controller's sound card (4 channels)
     *
     *     [[control]]
     *     midi   = { type = "note", channel = 1, number = 0x07 }   # or type = "cc"
     *     action = "deck.play"
     *     deck   = 1           # players count from 1 in the file
     *     mode   = "button"    # button, absolute, absolute14, relative; button if left out
     *     shift  = false
     *     invert = false       # absolute modes: 1 - value
     */
    struct Mapping
    {
            std::string              name;
            std::vector<std::string> match;
            std::string              audio;
            std::vector<Control>     controls;
            /// The file it was read from; empty for one made in code
            std::filesystem::path file;

            /// Whether the mapping is for a MIDI port of this name (one of match is part of it, ignoring case)
            [[nodiscard]] bool matches(std::string_view port) const;
    };

    /// A mapping from TOML text; the error says what is wrong and where
    [[nodiscard]] std::expected<Mapping, std::string> parseMapping(std::string_view text);
    /// A mapping from a file
    [[nodiscard]] std::expected<Mapping, std::string> readMapping(const std::filesystem::path &file);
    /// Every mapping (*.toml) in the folders, the first folder first; files that cannot be read are skipped and
    /// their errors added to errors
    [[nodiscard]] std::vector<Mapping> readMappings(const std::vector<std::filesystem::path> &folders,
                                                    std::vector<std::string>                 &errors);
} // namespace djw::core::control

#endif // DJW_CORE_CONTROL_MAPPING_HPP
