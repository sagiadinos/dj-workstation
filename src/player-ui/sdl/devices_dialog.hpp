// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_DEVICES_DIALOG_HPP
#define DJW_PLAYER_UI_SDL_DEVICES_DIALOG_HPP

#include "drop_down.hpp"
#include "i18n/translator.hpp"
#include "settings.hpp"
#include "text_button.hpp"
#include "view.hpp"
#include "widget.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace djw::player_ui::sdl
{
    /// Swallows every pointer over its area, so nothing below a modal dialog reacts
    class Blocker final : public Widget
    {
        public:
            void pointerDown(Point /*position*/) override {}
            void pointerMove(Point /*position*/) override {}
            void pointerUp(Point /*position*/) override {}
            void draw(Canvas & /*canvas*/) const override {}
    };

    /// What Settings > Devices offers to choose from
    struct DeviceChoices
    {
            /// The system's audio outputs by name
            std::vector<std::string> outputs;
            /// The MIDI ports there are now (stable names)
            std::vector<std::string> ports;
            /// The names of all controller mappings
            std::vector<std::string> mappings;
            /// For each port, the mapping it gets without a choice (empty: none matches)
            std::vector<std::string> port_defaults;
            /// A line shown above the buttons, e.g. what the last import made; empty for none
            std::string message{};
    };

    /**
     * Settings > Devices: a modal dialog box over the whole window (the window is dimmed and takes no input while it
     * is open).
     *
     * Audio: the output (Automatic: the sound card of a connected controller, else the system's default), stereo or
     * four channels (master 1/2, headphones 3/4), and the latency, shown in milliseconds at the device's sample rate.
     * These take effect at the next start, which it says. Controllers: one line per MIDI port with the mapping it
     * uses ("none" switches it off); the owner applies these at once.
     *
     * OK or Enter accept, Cancel or Escape leave the settings as they were; the owner reads takeOutcome() after the
     * update and removes the dialog between frames.
     */
    class DevicesDialog final : public View
    {
        public:
            enum class Outcome : std::uint8_t
            {
                Accepted,
                Cancelled,
                /// Import a Mixxx mapping: the owner closes the dialog, asks for the file and opens it again
                ImportMixxx
            };

            static constexpr float panel_width = 640.0F;
            /// The most controller lines shown
            static constexpr std::size_t max_ports = 4;

            /// settings: what is set now; sample_rate: the device's, for the milliseconds shown
            DevicesDialog(const Translator &translator, Settings settings, std::uint32_t sample_rate,
                          DeviceChoices choices = {});

            void               layout(const Rect &area) override;
            void               addTo(PointerRouter &router) override;
            void               update() override;
            void               draw(Canvas &canvas) const override;
            void               keyboard(const KeyboardInput &input) override;
            [[nodiscard]] bool wantsTextInput() const override { return false; }

            /// OK or Cancel, once; std::nullopt while the dialog is open
            [[nodiscard]] std::optional<Outcome> takeOutcome() noexcept { return std::exchange(outcome, std::nullopt); }
            /// The settings as chosen in the dialog
            [[nodiscard]] const Settings &chosen() const noexcept { return settings; }

            /// The text of a latency choice: its name and the milliseconds at the sample rate
            [[nodiscard]] std::string latencyText(std::size_t choice) const;

            [[nodiscard]] const DropDown &latencyList() const noexcept { return latency; }
            [[nodiscard]] const DropDown &outputList() const noexcept { return output; }
            [[nodiscard]] const DropDown &channelList() const noexcept { return channels; }
            /// The mapping choice of the port at index (in choices.ports)
            [[nodiscard]] const DropDown   &mappingList(std::size_t index) const { return *mapping_lists.at(index); }
            [[nodiscard]] const TextButton &okButton() const noexcept { return ok_button; }
            [[nodiscard]] const TextButton &cancelButton() const noexcept { return cancel_button; }
            [[nodiscard]] const TextButton &importButton() const noexcept { return import_button; }
            [[nodiscard]] const Rect       &panel() const noexcept { return panel_area; }
            /// How high the panel wants to be for what it shows
            [[nodiscard]] float panelHeight() const noexcept;

        private:
            /// The mapping a port uses now: its choice in settings, else its default
            [[nodiscard]] std::string mappingOf(std::size_t port) const;
            void                      takeChoices();

            const Translator                      *translator;
            Settings                               settings;
            std::uint32_t                          sample_rate;
            DeviceChoices                          choices;
            std::optional<Outcome>                 outcome;
            Rect                                   window_area;
            Rect                                   panel_area;
            Rect                                   title_area;
            Rect                                   output_label;
            Rect                                   channels_label;
            Rect                                   latency_label;
            Rect                                   hint_area;
            Rect                                   controllers_area;
            std::vector<Rect>                      port_labels;
            Blocker                                blocker;
            DropDown                               output;
            DropDown                               channels;
            DropDown                               latency;
            std::vector<std::unique_ptr<DropDown>> mapping_lists;
            TextButton                             ok_button;
            TextButton                             cancel_button;
            TextButton                             import_button;
            Rect                                   message_area;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_DEVICES_DIALOG_HPP
