// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_SOUND_DIALOG_HPP
#define DJW_PLAYER_UI_SDL_SOUND_DIALOG_HPP

#include "api/time_stretch.hpp"
#include "devices_dialog.hpp"
#include "drop_down.hpp"
#include "i18n/translator.hpp"
#include "settings.hpp"
#include "text_button.hpp"
#include "view.hpp"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace djw::player_ui::sdl
{
    /**
     * Settings > Sound: a modal dialog box with the keylock quality. Standard takes little computing power
     * (SoundTouch), High more (Signalsmith Stretch); the libraries are not named. Only the qualities built in are
     * offered. The choice takes effect at the next start, which it says.
     *
     * OK or Enter accept, Cancel or Escape leave the settings as they were; the owner reads takeOutcome() after the
     * update, keeps the settings and removes the dialog between frames.
     */
    class SoundDialog final : public View
    {
        public:
            enum class Outcome : std::uint8_t
            {
                Accepted,
                Cancelled
            };

            static constexpr float panel_width = 560.0F;

            /// settings: what is set now; offered: the keylock libraries built in, in the order shown
            SoundDialog(const Translator &translator, Settings settings,
                        std::vector<core::api::TimeStretch> offered = {core::api::TimeStretch::SoundTouch});

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

            [[nodiscard]] const DropDown   &qualityList() const noexcept { return quality; }
            [[nodiscard]] const TextButton &okButton() const noexcept { return ok_button; }
            [[nodiscard]] const TextButton &cancelButton() const noexcept { return cancel_button; }
            [[nodiscard]] const Rect       &panel() const noexcept { return panel_area; }

        private:
            const Translator                   *translator;
            Settings                            settings;
            std::vector<core::api::TimeStretch> offered;
            std::optional<Outcome>              outcome;
            Rect                                window_area;
            Rect                                panel_area;
            Rect                                title_area;
            Rect                                quality_label;
            Rect                                hint_area;
            Blocker                             blocker;
            DropDown                            quality;
            TextButton                          ok_button;
            TextButton                          cancel_button;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_SOUND_DIALOG_HPP
