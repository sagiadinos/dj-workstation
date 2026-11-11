// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_I18N_MESSAGE_FORMAT_HPP
#define DJW_PLAYER_UI_SDL_I18N_MESSAGE_FORMAT_HPP

#include "i18n/language.hpp"

#include <concepts>
#include <expected>
#include <span>
#include <string>
#include <string_view>

namespace djw::player_ui::sdl
{
    /// A named value for a message: a whole number or a text
    class MessageArg
    {
        public:
            MessageArg(std::string_view name, std::string_view text) : arg_name{name}, arg_text{text} {}

            template <std::integral T>
                requires(!std::same_as<T, bool>)
            MessageArg(std::string_view name, T number)
                : arg_name{name}, arg_number{static_cast<long long>(number)}, is_number{true}
            {
            }

            [[nodiscard]] std::string_view   name() const noexcept { return arg_name; }
            [[nodiscard]] bool               isNumber() const noexcept { return is_number; }
            [[nodiscard]] long long          number() const noexcept { return arg_number; }
            [[nodiscard]] const std::string &text() const noexcept { return arg_text; }

        private:
            std::string_view arg_name;
            std::string      arg_text;
            long long        arg_number = 0;
            bool             is_number  = false;
    };

    /**
     * Fills a message, written in the ICU MessageFormat subset the catalogs use:
     *
     *   plain text                       copied
     *   {name}                           the argument as text or number
     *   {n, plural, one {# track} other {# tracks}}
     *                                    the branch of the plural form of the number n in the language, # is the
     *                                    number; exact values as =0 {...} win; other is required
     *   ''  '{'  '}'                     a quote, and quoted braces (an apostrophe elsewhere is just an apostrophe)
     *
     * An error names what is wrong with the pattern or the arguments.
     */
    [[nodiscard]] std::expected<std::string, std::string> formatMessage(Language language, std::string_view pattern,
                                                                        std::span<const MessageArg> args);
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_I18N_MESSAGE_FORMAT_HPP
