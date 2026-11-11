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
#include <optional>
#include <string>

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

    /**
     * Settings > Devices: a modal dialog box over the whole window (the window is dimmed and takes no input while it
     * is open). For now it sets the latency, as one of latency_choices shown in milliseconds at the device's sample
     * rate; it takes effect at the next start, which it says. Later the audio driver and device are chosen here.
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
                Cancelled
            };

            static constexpr float panel_width  = 520.0F;
            static constexpr float panel_height = 300.0F;

            /// settings: what is set now; sample_rate: the device's, for the milliseconds shown
            DevicesDialog(const Translator &translator, const Settings &settings, std::uint32_t sample_rate);

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

            [[nodiscard]] const DropDown   &latencyList() const noexcept { return latency; }
            [[nodiscard]] const TextButton &okButton() const noexcept { return ok_button; }
            [[nodiscard]] const TextButton &cancelButton() const noexcept { return cancel_button; }
            [[nodiscard]] const Rect       &panel() const noexcept { return panel_area; }

        private:
            const Translator      *translator;
            Settings               settings;
            std::uint32_t          sample_rate;
            std::optional<Outcome> outcome;
            Rect                   window_area;
            Rect                   panel_area;
            Rect                   label_area;
            Rect                   hint_area;
            Blocker                blocker;
            DropDown               latency;
            TextButton             ok_button;
            TextButton             cancel_button;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_DEVICES_DIALOG_HPP
