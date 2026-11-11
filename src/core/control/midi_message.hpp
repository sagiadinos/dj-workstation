// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_CONTROL_MIDI_MESSAGE_HPP
#define DJW_CORE_CONTROL_MIDI_MESSAGE_HPP

#include <cstdint>
#include <optional>

namespace djw::core::control
{
    /// What a control of a controller sends
    enum class MidiKind : std::uint8_t
    {
        Note,         ///< note on and off: buttons and pads
        ControlChange ///< faders, knobs, encoders
    };

    /// One short MIDI message (status and up to two data bytes); system messages are not used
    struct MidiMessage
    {
            std::uint8_t status = 0;
            std::uint8_t data1  = 0;
            std::uint8_t data2  = 0;

            /// Note (on or off) or control change; std::nullopt for any other message
            [[nodiscard]] std::optional<MidiKind> kind() const noexcept
            {
                switch (status & 0xF0U)
                {
                    case 0x80U:
                    case 0x90U:
                        return MidiKind::Note;
                    case 0xB0U:
                        return MidiKind::ControlChange;
                    default:
                        return std::nullopt;
                }
            }
            /// MIDI channel, 1 to 16
            [[nodiscard]] int channel() const noexcept { return static_cast<int>(status & 0x0FU) + 1; }
            /// A note off, or a note on with velocity 0, which means the same
            [[nodiscard]] bool noteOff() const noexcept
            {
                return (status & 0xF0U) == 0x80U || ((status & 0xF0U) == 0x90U && data2 == 0);
            }

            bool operator==(const MidiMessage &) const = default;
    };
} // namespace djw::core::control

#endif // DJW_CORE_CONTROL_MIDI_MESSAGE_HPP
