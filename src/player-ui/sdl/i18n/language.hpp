// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_I18N_LANGUAGE_HPP
#define DJW_PLAYER_UI_SDL_I18N_LANGUAGE_HPP

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace djw::player_ui::sdl
{
    /// The languages of the user interface
    enum class Language : std::uint8_t
    {
        English,
        German,
        French,
        Spanish,
        Greek,
        Russian
    };

    inline constexpr std::array<Language, 6> languages{Language::English, Language::German, Language::French,
                                                       Language::Spanish, Language::Greek,  Language::Russian};

    /// ISO 639-1 code, e.g. "de"
    [[nodiscard]] std::string_view languageCode(Language language);

    /// The name of the language in the language itself, e.g. "Deutsch"
    [[nodiscard]] std::string_view languageName(Language language);

    /// The language for a code as locales spell it: "de", "de_DE", "de-AT", "de_DE.UTF-8"; std::nullopt if unsupported
    [[nodiscard]] std::optional<Language> languageFromCode(std::string_view code);

    /// True for languages written from right to left; the layouts mirror for them
    [[nodiscard]] bool isRightToLeft(Language language);

    /// The plural forms of the CLDR rules
    enum class PluralCategory : std::uint8_t
    {
        Zero,
        One,
        Two,
        Few,
        Many,
        Other
    };

    /// The plural form of the language for a whole number (the sign is ignored)
    [[nodiscard]] PluralCategory pluralCategory(Language language, long long number);

    /// "one", "few", ... as message patterns spell them
    [[nodiscard]] std::string_view pluralCategoryName(PluralCategory category);
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_I18N_LANGUAGE_HPP
