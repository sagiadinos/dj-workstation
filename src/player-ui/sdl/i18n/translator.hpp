// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_I18N_TRANSLATOR_HPP
#define DJW_PLAYER_UI_SDL_I18N_TRANSLATOR_HPP

#include "i18n/language.hpp"
#include "i18n/message_format.hpp"

#include <initializer_list>
#include <string>
#include <string_view>
#include <unordered_map>

namespace djw::player_ui::sdl
{
    /**
     * The texts of the user interface in the current language.
     *
     * tr() looks the key up in the catalog of the language, then in English, and fills the message with the arguments.
     * A key that is nowhere (or a message that does not format) comes back as the key itself, so the mistake shows.
     * Views ask for their texts again on every frame, so setLanguage() takes effect at once everywhere.
     */
    class Translator
    {
        public:
            explicit Translator(Language language = Language::English);

            [[nodiscard]] Language language() const noexcept { return current; }
            void                   setLanguage(Language language);
            [[nodiscard]] bool     rightToLeft() const noexcept { return isRightToLeft(current); }

            [[nodiscard]] std::string tr(std::string_view key, std::initializer_list<MessageArg> args = {}) const;

            /// The message of key as the catalog has it, empty if there is none; for tests
            [[nodiscard]] std::string_view pattern(std::string_view key) const;

        private:
            using Index = std::unordered_map<std::string_view, std::string_view>;

            Language current;
            Index    texts;    // the current language
            Index    fallback; // English
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_I18N_TRANSLATOR_HPP
