// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "control/controller.hpp"

#include <algorithm>

namespace djw::core::control
{
    namespace
    {
        /// An absolute value as the control means it
        float scaled(const Control &control, float value) noexcept
        {
            return control.invert ? 1.0F - value : value;
        }
    } // namespace

    const Control *Controller::find(MidiKind kind, int channel, std::uint8_t number, bool shifted) const
    {
        const auto found =
            std::ranges::find_if(map.controls,
                                 [&](const Control &control)
                                 {
                                     return control.kind == kind && control.channel == channel &&
                                            control.number == number &&
                                            (control.shift == shifted || control.action == Action::Shift);
                                 });
        return found != map.controls.end() ? &*found : nullptr;
    }

    std::vector<ControlEvent> Controller::handle(const MidiMessage &message)
    {
        const auto kind = message.kind();
        if (!kind)
        {
            return {};
        }
        const int channel = message.channel();
        const Key key{*kind, channel, message.data1};

        // A button let go: as what it was pressed as
        const bool released = *kind == MidiKind::Note ? message.noteOff() : message.data2 == 0;
        if (released)
        {
            if (const auto held = pressed.find(key); held != pressed.end())
            {
                const Control *control = held->second;
                pressed.erase(held);
                if (control->action == Action::Shift)
                {
                    shift = false;
                    return {};
                }
                return {ControlEvent{.action = control->action, .deck = control->deck, .pressed = false}};
            }
        }

        // The low part of a 14-bit control: number - 32 is its high part
        if (*kind == MidiKind::ControlChange && message.data1 >= 32 && message.data1 < 64)
        {
            const auto     high_number = static_cast<std::uint8_t>(message.data1 - 32);
            const Control *control     = find(MidiKind::ControlChange, channel, high_number, shift);
            if (control != nullptr && control->mode == Mode::Absolute14)
            {
                std::uint16_t &value = wide[Key{MidiKind::ControlChange, channel, high_number}];
                value                = static_cast<std::uint16_t>((value & 0x3F80U) | (message.data2 & 0x7FU));
                return {ControlEvent{.action = control->action,
                                     .deck   = control->deck,
                                     .value  = scaled(*control, static_cast<float>(value) / 16383.0F)}};
            }
        }

        const Control *control = find(*kind, channel, message.data1, shift);
        if (control == nullptr)
        {
            return {};
        }
        switch (control->mode)
        {
            case Mode::Button:
                if (released)
                {
                    return {}; // released, but never seen pressed
                }
                pressed[key] = control;
                if (control->action == Action::Shift)
                {
                    shift = true;
                    return {};
                }
                return {ControlEvent{.action = control->action, .deck = control->deck, .pressed = true}};
            case Mode::Absolute:
                return {ControlEvent{.action = control->action,
                                     .deck   = control->deck,
                                     .value  = scaled(*control, static_cast<float>(message.data2) / 127.0F)}};
            case Mode::Absolute14:
            {
                std::uint16_t &value = wide[key];
                value                = static_cast<std::uint16_t>(((message.data2 & 0x7FU) << 7U) | (value & 0x7FU));
                return {ControlEvent{.action = control->action,
                                     .deck   = control->deck,
                                     .value  = scaled(*control, static_cast<float>(value) / 16383.0F)}};
            }
            case Mode::Relative:
            {
                const int steps = message.data2 < 64 ? message.data2 : message.data2 - 128;
                if (steps == 0)
                {
                    return {};
                }
                return {ControlEvent{.action = control->action, .deck = control->deck, .steps = steps}};
            }
        }
        return {};
    }
} // namespace djw::core::control
