// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_CONFIRM_DIALOG_HPP
#define DJW_PLAYER_UI_SDL_CONFIRM_DIALOG_HPP

#include "devices_dialog.hpp"
#include "i18n/translator.hpp"
#include "text_button.hpp"
#include "view.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace djw::player_ui::sdl
{
    /**
     * A modal question before something that cannot be undone, e.g. quitting while a track plays: a title, a message
     * and two buttons, the one that does it and Cancel. Cancel is the default: Enter and Escape cancel, only a tap on
     * the other button confirms. The texts are catalog keys, so they follow the language.
     *
     * The owner reads takeOutcome() after the update and removes the dialog between frames.
     */
    class ConfirmDialog final : public View
    {
        public:
            enum class Outcome : std::uint8_t
            {
                Confirmed,
                Cancelled
            };

            static constexpr float panel_width = 480.0F;

            /// title_key, message_key: the texts; confirm_key: the label of the button that does it
            ConfirmDialog(const Translator &translator, std::string title_key, std::string message_key,
                          std::string confirm_key);

            void               layout(const Rect &area) override;
            void               addTo(PointerRouter &router) override;
            void               update() override;
            void               draw(Canvas &canvas) const override;
            void               keyboard(const KeyboardInput &input) override;
            [[nodiscard]] bool wantsTextInput() const override { return false; }

            /// Confirmed or Cancelled, once; std::nullopt while the dialog is open
            [[nodiscard]] std::optional<Outcome> takeOutcome() noexcept { return std::exchange(outcome, std::nullopt); }

            [[nodiscard]] const TextButton &confirmButton() const noexcept { return confirm_button; }
            [[nodiscard]] const TextButton &cancelButton() const noexcept { return cancel_button; }
            [[nodiscard]] const Rect       &panel() const noexcept { return panel_area; }

        private:
            const Translator      *translator;
            std::string            title_key;
            std::string            message_key;
            std::string            confirm_key;
            std::optional<Outcome> outcome;
            Rect                   window_area;
            Rect                   panel_area;
            Rect                   title_area;
            Rect                   message_area;
            Blocker                blocker;
            TextButton             confirm_button;
            TextButton             cancel_button;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_CONFIRM_DIALOG_HPP
