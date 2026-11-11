// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_CONTROL_MIDI_SYSTEM_HPP
#define DJW_CORE_CONTROL_MIDI_SYSTEM_HPP

#include "control/midi_message.hpp"

#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace djw::core::control
{
    /// An open MIDI input port; it closes when destroyed, after which no message arrives any more
    class MidiPort
    {
        public:
            MidiPort()                            = default;
            MidiPort(const MidiPort &)            = delete;
            MidiPort &operator=(const MidiPort &) = delete;
            MidiPort(MidiPort &&)                 = delete;
            MidiPort &operator=(MidiPort &&)      = delete;
            virtual ~MidiPort()                   = default;
    };

    /// The MIDI inputs of the system (RtMidi on the desktop); tests use a fake
    class MidiSystem
    {
        public:
            using Receive = std::function<void(const MidiMessage &message)>;

            MidiSystem()                              = default;
            MidiSystem(const MidiSystem &)            = delete;
            MidiSystem &operator=(const MidiSystem &) = delete;
            MidiSystem(MidiSystem &&)                 = delete;
            MidiSystem &operator=(MidiSystem &&)      = delete;
            virtual ~MidiSystem()                     = default;

            /// The names of the MIDI input ports there are now
            [[nodiscard]] virtual std::vector<std::string> ports() = 0;
            /// Opens the port of that name; its messages go to receive, on a thread of the MIDI system
            [[nodiscard]] virtual std::expected<std::unique_ptr<MidiPort>, std::string> open(std::string_view port,
                                                                                             Receive receive) = 0;
    };

    /// The system's MIDI (RtMidi); a system without MIDI gives one without ports
    [[nodiscard]] std::unique_ptr<MidiSystem> createMidiSystem();
} // namespace djw::core::control

#endif // DJW_CORE_CONTROL_MIDI_SYSTEM_HPP
