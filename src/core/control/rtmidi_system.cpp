// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "control/midi_system.hpp"

#include <RtMidi.h>

#include <utility>

namespace djw::core::control
{
    namespace
    {
        constexpr const char *client_name = "djw";

        /// Errors go to the caller as values, not to the console
        void quiet(RtMidiError::Type /*type*/, const std::string & /*text*/, void * /*user*/) {}

        class RtMidiPort final : public MidiPort
        {
            public:
                RtMidiPort(std::unique_ptr<RtMidiIn> input, MidiSystem::Receive receive)
                    : input{std::move(input)}, receive{std::move(receive)}
                {
                    this->input->setCallback(&RtMidiPort::arrived, this);
                    this->input->ignoreTypes(true, true, true); // no system exclusive, timing or active sensing
                }

                ~RtMidiPort() override
                {
                    input->cancelCallback();
                    input->closePort();
                }

                RtMidiPort(const RtMidiPort &)            = delete;
                RtMidiPort &operator=(const RtMidiPort &) = delete;
                RtMidiPort(RtMidiPort &&)                 = delete;
                RtMidiPort &operator=(RtMidiPort &&)      = delete;

            private:
                static void arrived(double /*delta*/, std::vector<unsigned char> *bytes, void *self)
                {
                    if (bytes == nullptr || bytes->empty())
                    {
                        return;
                    }
                    MidiMessage message{.status = bytes->at(0)};
                    if (bytes->size() > 1)
                    {
                        message.data1 = bytes->at(1);
                    }
                    if (bytes->size() > 2)
                    {
                        message.data2 = bytes->at(2);
                    }
                    static_cast<RtMidiPort *>(self)->receive(message);
                }

                std::unique_ptr<RtMidiIn> input;
                MidiSystem::Receive       receive;
        };

        class RtMidiSystem final : public MidiSystem
        {
            public:
                std::vector<std::string> ports() override
                {
                    std::vector<std::string> names;
                    try
                    {
                        RtMidiIn probe{RtMidi::UNSPECIFIED, client_name};
                        probe.setErrorCallback(&quiet);
                        const unsigned int count = probe.getPortCount();
                        for (unsigned int index = 0; index < count; ++index)
                        {
                            names.push_back(probe.getPortName(index));
                        }
                    }
                    catch (const RtMidiError &)
                    {
                        names.clear(); // no MIDI on this system (no sequencer device): no ports
                    }
                    return names;
                }

                std::expected<std::unique_ptr<MidiPort>, std::string> open(std::string_view port,
                                                                           Receive          receive) override
                {
                    try
                    {
                        auto input = std::make_unique<RtMidiIn>(RtMidi::UNSPECIFIED, client_name);
                        input->setErrorCallback(&quiet);
                        const unsigned int count = input->getPortCount();
                        for (unsigned int index = 0; index < count; ++index)
                        {
                            if (input->getPortName(index) == port)
                            {
                                input->openPort(index, client_name);
                                return std::make_unique<RtMidiPort>(std::move(input), std::move(receive));
                            }
                        }
                        return std::unexpected{"no MIDI port " + std::string{port}};
                    }
                    catch (const RtMidiError &error)
                    {
                        return std::unexpected{"cannot open MIDI port " + std::string{port} + ": " +
                                               error.getMessage()};
                    }
                }
        };
    } // namespace

    std::unique_ptr<MidiSystem> createMidiSystem()
    {
        return std::make_unique<RtMidiSystem>();
    }
} // namespace djw::core::control
