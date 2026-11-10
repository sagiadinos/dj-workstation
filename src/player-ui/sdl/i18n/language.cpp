// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "i18n/language.hpp"

#include <algorithm>
#include <cctype>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr unsigned long long million = 1'000'000;
    }

    std::string_view languageCode(Language language)
    {
        switch (language)
        {
            case Language::German:
                return "de";
            case Language::French:
                return "fr";
            case Language::Spanish:
                return "es";
            case Language::Greek:
                return "el";
            case Language::Russian:
                return "ru";
            case Language::English:
                break;
        }
        return "en";
    }

    std::string_view languageName(Language language)
    {
        switch (language)
        {
            case Language::German:
                return "Deutsch";
            case Language::French:
                return "Français";
            case Language::Spanish:
                return "Español";
            case Language::Greek:
                return "Ελληνικά";
            case Language::Russian:
                return "Русский";
            case Language::English:
                break;
        }
        return "English";
    }

    std::optional<Language> languageFromCode(std::string_view code)
    {
        // Only the language part: "de_DE.UTF-8" and "de-AT" are German
        const std::size_t end = code.find_first_of("_-.@");
        code                  = code.substr(0, end);
        for (const Language language : languages)
        {
            const std::string_view known = languageCode(language);
            if (std::ranges::equal(code, known,
                                   [](char left, char right)
                                   { return std::tolower(static_cast<unsigned char>(left)) == right; }))
            {
                return language;
            }
        }
        return std::nullopt;
    }

    bool isRightToLeft(Language /*language*/)
    {
        return false; // none of the languages so far; Arabic and Hebrew will be the first
    }

    PluralCategory pluralCategory(Language language, long long number)
    {
        const unsigned long long n =
            number < 0 ? 0ULL - static_cast<unsigned long long>(number) : static_cast<unsigned long long>(number);
        switch (language)
        {
            case Language::French:
                if (n <= 1)
                {
                    return PluralCategory::One;
                }
                return n % million == 0 ? PluralCategory::Many : PluralCategory::Other;
            case Language::Spanish:
                if (n == 1)
                {
                    return PluralCategory::One;
                }
                return n != 0 && n % million == 0 ? PluralCategory::Many : PluralCategory::Other;
            case Language::Russian:
            {
                const unsigned long long last_digit = n % 10;
                const unsigned long long last_two   = n % 100;
                if (last_digit == 1 && last_two != 11)
                {
                    return PluralCategory::One;
                }
                if (last_digit >= 2 && last_digit <= 4 && (last_two < 12 || last_two > 14))
                {
                    return PluralCategory::Few;
                }
                return PluralCategory::Many;
            }
            case Language::English:
            case Language::German:
            case Language::Greek:
                break;
        }
        return n == 1 ? PluralCategory::One : PluralCategory::Other;
    }

    std::string_view pluralCategoryName(PluralCategory category)
    {
        switch (category)
        {
            case PluralCategory::Zero:
                return "zero";
            case PluralCategory::One:
                return "one";
            case PluralCategory::Two:
                return "two";
            case PluralCategory::Few:
                return "few";
            case PluralCategory::Many:
                return "many";
            case PluralCategory::Other:
                break;
        }
        return "other";
    }
} // namespace djw::player_ui::sdl
