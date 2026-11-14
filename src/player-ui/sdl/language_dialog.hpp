// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_LANGUAGE_DIALOG_HPP
#define DJW_PLAYER_UI_SDL_LANGUAGE_DIALOG_HPP

#include "devices_dialog.hpp"
#include "drop_down.hpp"
#include "i18n/language.hpp"
#include "i18n/translator.hpp"
#include "text_button.hpp"
#include "view.hpp"

#include <cstdint>
#include <optional>
#include <utility>

namespace djw::player_ui::sdl
{
    /**
     * Settings > Language: a modal dialog box with one list of the languages, each in its own name.
     *
     * OK or Enter take the language chosen, Cancel or Escape keep the one in use; the owner reads takeOutcome() after
     * the update, switches the language and removes the dialog between frames.
     */
    class LanguageDialog final : public View
    {
        public:
            enum class Outcome : std::uint8_t
            {
                Accepted,
                Cancelled
            };

            static constexpr float panel_width = 420.0F;

            /// current: the language in use, chosen when the dialog opens
            LanguageDialog(const Translator &translator, Language current);

            void               layout(const Rect &area) override;
            void               addTo(PointerRouter &router) override;
            void               update() override;
            void               draw(Canvas &canvas) const override;
            void               keyboard(const KeyboardInput &input) override;
            [[nodiscard]] bool wantsTextInput() const override { return false; }

            /// OK or Cancel, once; std::nullopt while the dialog is open
            [[nodiscard]] std::optional<Outcome> takeOutcome() noexcept { return std::exchange(outcome, std::nullopt); }
            /// The language chosen in the dialog
            [[nodiscard]] Language chosen() const noexcept { return language; }

            [[nodiscard]] const DropDown   &languageList() const noexcept { return list; }
            [[nodiscard]] const TextButton &okButton() const noexcept { return ok_button; }
            [[nodiscard]] const TextButton &cancelButton() const noexcept { return cancel_button; }
            [[nodiscard]] const Rect       &panel() const noexcept { return panel_area; }

        private:
            const Translator      *translator;
            Language               language;
            std::optional<Outcome> outcome;
            Rect                   window_area;
            Rect                   panel_area;
            Rect                   title_area;
            Blocker                blocker;
            DropDown               list;
            TextButton             ok_button;
            TextButton             cancel_button;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_LANGUAGE_DIALOG_HPP
