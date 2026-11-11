// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_CONTROL_CONTROLLER_HUB_HPP
#define DJW_CORE_CONTROL_CONTROLLER_HUB_HPP

#include "api/controllers.hpp"
#include "control/controller.hpp"
#include "control/mapping.hpp"
#include "control/midi_system.hpp"

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace djw::core::control
{
    /**
     * The connected controllers: opens the MIDI ports that have a mapping and collects their control events for the
     * frontend, which takes them once per frame (poll()) and carries them out.
     *
     * Messages arrive on threads of the MIDI system; they are turned into events there and queued under a lock, so
     * the frontend's thread never waits for MIDI. Never touches the audio thread.
     */
    class ControllerHub final : public api::Controllers
    {
        public:
            /// A port's name without the numbers ALSA adds at the end ("Starlight MIDI 1 24:0" → "Starlight MIDI 1"),
            /// which change when the device is plugged in again; other names stay as they are
            [[nodiscard]] static std::string stablePortName(std::string_view port);
            /// A port of the system that is no controller: ALSA's "Midi Through", which passes on what is sent to it
            [[nodiscard]] static bool isSystemPort(std::string_view port);

            ControllerHub(std::unique_ptr<MidiSystem> midi, std::vector<Mapping> mappings);
            ~ControllerHub() override;
            ControllerHub(const ControllerHub &)            = delete;
            ControllerHub &operator=(const ControllerHub &) = delete;
            ControllerHub(ControllerHub &&)                 = delete;
            ControllerHub &operator=(ControllerHub &&)      = delete;

            /**
             * Closes all ports and opens again every port with a mapping: the one assigned to it, or else the first
             * mapping that matches its name. Errors (a port that cannot be opened, an assigned mapping that does not
             * exist) are returned; the other ports still work.
             */
            std::vector<std::string> connect(const Assignments &assigned = {}) override;

            /// The events since the last call, in the order they came
            [[nodiscard]] std::vector<ControlEvent> poll() override;

            [[nodiscard]] std::vector<std::string> ports() const override;
            [[nodiscard]] std::vector<std::string> mappingNames() const override;
            [[nodiscard]] std::string              mappingFor(const std::string &port) const override;
            [[nodiscard]] std::vector<Connection>  connections() const override;
            [[nodiscard]] std::string              controllerAudio() const override;

            [[nodiscard]] const std::vector<Mapping> &mappings() const noexcept { return all; }
            /// The mapping a port gets without an assignment: the first that matches its name; nullptr for none
            [[nodiscard]] const Mapping *mappingMatching(const std::string &port) const;
            /// The mapping of that name, nullptr if there is none
            [[nodiscard]] const Mapping *mappingNamed(const std::string &name) const;

        private:
            /// One open port with its controller; it lives where the MIDI thread's callback finds it
            struct Open
            {
                    std::string               port;
                    Controller                controller;
                    std::unique_ptr<MidiPort> input; ///< closed first, then the controller goes
            };

            void received(Open &open, const MidiMessage &message);
            void closeAll();

            std::unique_ptr<MidiSystem>        midi;
            std::vector<Mapping>               all;
            std::vector<std::unique_ptr<Open>> open_ports;
            mutable std::mutex                 lock;
            std::vector<ControlEvent>          queued;
    };
} // namespace djw::core::control

#endif // DJW_CORE_CONTROL_CONTROLLER_HUB_HPP
