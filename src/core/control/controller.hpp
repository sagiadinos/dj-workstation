// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_CONTROL_CONTROLLER_HPP
#define DJW_CORE_CONTROL_CONTROLLER_HPP

#include "control/mapping.hpp"
#include "control/midi_message.hpp"

#include <cstddef>
#include <map>
#include <optional>
#include <tuple>
#include <vector>

namespace djw::core::control
{
    /// A control of a controller was used
    struct ControlEvent
    {
            Action                     action = Action::DeckPlay;
            std::optional<std::size_t> deck; ///< the player, from 0
            /// Button: pressed (true) or released; the other modes: true
            bool pressed = true;
            /// Absolute modes: 0 to 1
            float value = 0.0F;
            /// Relative mode: steps, negative back
            int steps = 0;

            bool operator==(const ControlEvent &) const = default;
    };

    /**
     * One connected controller: turns its MIDI messages into control events through its mapping.
     *
     * Shift is held while the control mapped to it is pressed; then the controls with shift = true apply. A button
     * is released as what it was pressed as, even if Shift changed in between (a CUE pressed without Shift is let go
     * as CUE). A 14-bit control sends its high part, then its low part; each of them gives an event with the value
     * of both as last known.
     */
    class Controller
    {
        public:
            explicit Controller(Mapping mapping) : map{std::move(mapping)} {}

            /// The events for one message; none for a message the mapping does not know
            [[nodiscard]] std::vector<ControlEvent> handle(const MidiMessage &message);

            [[nodiscard]] const Mapping &mapping() const noexcept { return map; }
            [[nodiscard]] bool           shiftHeld() const noexcept { return shift; }

        private:
            using Key = std::tuple<MidiKind, int, std::uint8_t>; ///< kind, channel, number

            [[nodiscard]] const Control *find(MidiKind kind, int channel, std::uint8_t number, bool shifted) const;

            Mapping                        map;
            bool                           shift = false;
            std::map<Key, const Control *> pressed; ///< buttons held, by what they send
            std::map<Key, std::uint16_t>   wide;    ///< the last 14-bit value of each control, by its high part
    };
} // namespace djw::core::control

#endif // DJW_CORE_CONTROL_CONTROLLER_HPP
