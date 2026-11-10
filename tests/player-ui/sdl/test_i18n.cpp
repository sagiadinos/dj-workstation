// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "i18n/catalog.hpp"
#include "i18n/translator.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

using namespace djw::player_ui::sdl;
using Category = PluralCategory;

TEST_CASE("Language: codes and names, locales as the system spells them")
{
    CHECK(languageCode(Language::German) == "de");
    CHECK(languageName(Language::Greek) == "Ελληνικά");
    CHECK(languageFromCode("de") == Language::German);
    CHECK(languageFromCode("de_DE.UTF-8") == Language::German);
    CHECK(languageFromCode("RU-ru") == Language::Russian);
    CHECK(languageFromCode("fr@euro") == Language::French);
    CHECK_FALSE(languageFromCode("ja").has_value());
    CHECK_FALSE(languageFromCode("").has_value());
    for (const Language language : languages)
    {
        CHECK(languageFromCode(languageCode(language)) == language);
        CHECK_FALSE(isRightToLeft(language));
    }
}

TEST_CASE("pluralCategory: English, German and Greek know one and other")
{
    for (const Language language : {Language::English, Language::German, Language::Greek})
    {
        CHECK(pluralCategory(language, 1) == Category::One);
        CHECK(pluralCategory(language, 0) == Category::Other);
        CHECK(pluralCategory(language, 2) == Category::Other);
        CHECK(pluralCategory(language, 21) == Category::Other);
    }
}

TEST_CASE("pluralCategory: French counts 0 and 1 as one, millions are many")
{
    CHECK(pluralCategory(Language::French, 0) == Category::One);
    CHECK(pluralCategory(Language::French, 1) == Category::One);
    CHECK(pluralCategory(Language::French, 2) == Category::Other);
    CHECK(pluralCategory(Language::French, 1'000'000) == Category::Many);
    CHECK(pluralCategory(Language::French, 2'000'000) == Category::Many);
    CHECK(pluralCategory(Language::French, 1'000'001) == Category::Other);
}

TEST_CASE("pluralCategory: Spanish has one for 1 only and many for millions")
{
    CHECK(pluralCategory(Language::Spanish, 1) == Category::One);
    CHECK(pluralCategory(Language::Spanish, 0) == Category::Other);
    CHECK(pluralCategory(Language::Spanish, 1'000'000) == Category::Many);
}

TEST_CASE("pluralCategory: Russian has one, few and many")
{
    const auto russian = [](long long n) { return pluralCategory(Language::Russian, n); };
    CHECK(russian(1) == Category::One);
    CHECK(russian(21) == Category::One);
    CHECK(russian(101) == Category::One);
    CHECK(russian(11) == Category::Many);
    CHECK(russian(111) == Category::Many);
    CHECK(russian(2) == Category::Few);
    CHECK(russian(4) == Category::Few);
    CHECK(russian(23) == Category::Few);
    CHECK(russian(12) == Category::Many);
    CHECK(russian(14) == Category::Many);
    CHECK(russian(0) == Category::Many);
    CHECK(russian(5) == Category::Many);
    CHECK(russian(20) == Category::Many);
    CHECK(russian(-1) == Category::One);
}

namespace
{
    std::string format(Language language, std::string_view pattern, std::initializer_list<MessageArg> args = {})
    {
        const auto done = formatMessage(language, pattern, std::span<const MessageArg>{args.begin(), args.size()});
        REQUIRE(done.has_value());
        return *done;
    }

    std::string formatError(std::string_view pattern, std::initializer_list<MessageArg> args = {})
    {
        const auto done =
            formatMessage(Language::English, pattern, std::span<const MessageArg>{args.begin(), args.size()});
        REQUIRE_FALSE(done.has_value());
        return done.error();
    }
} // namespace

TEST_CASE("formatMessage: plain text and arguments")
{
    CHECK(format(Language::English, "Hello") == "Hello");
    CHECK(format(Language::English, "Hi {name}, {n} new", {{"name", "Niko"}, {"n", 3}}) == "Hi Niko, 3 new");
    CHECK(format(Language::German, "Größe: {n}", {{"n", std::size_t{12}}}) == "Größe: 12");
    CHECK(format(Language::Greek, "Ελληνικά {x}", {{"x", "ΟΚ"}}) == "Ελληνικά ΟΚ");
}

TEST_CASE("formatMessage: plural picks the branch of the language and # is the number")
{
    constexpr std::string_view tracks = "{n, plural, one {# track} other {# tracks}}";
    CHECK(format(Language::English, tracks, {{"n", 1}}) == "1 track");
    CHECK(format(Language::English, tracks, {{"n", 0}}) == "0 tracks");
    CHECK(format(Language::English, tracks, {{"n", 340}}) == "340 tracks");

    constexpr std::string_view russian = "{n, plural, one {# трек} few {# трека} many {# треков} other {# трека}}";
    CHECK(format(Language::Russian, russian, {{"n", 1}}) == "1 трек");
    CHECK(format(Language::Russian, russian, {{"n", 3}}) == "3 трека");
    CHECK(format(Language::Russian, russian, {{"n", 5}}) == "5 треков");
    CHECK(format(Language::Russian, russian, {{"n", 11}}) == "11 треков");
    CHECK(format(Language::Russian, russian, {{"n", 22}}) == "22 трека");
}

TEST_CASE("formatMessage: a missing branch falls back to other, an exact value wins")
{
    constexpr std::string_view pattern = "{n, plural, =0 {none} one {one} other {# many}}";
    CHECK(format(Language::English, pattern, {{"n", 0}}) == "none");
    CHECK(format(Language::English, pattern, {{"n", 1}}) == "one");
    CHECK(format(Language::English, pattern, {{"n", 7}}) == "7 many");
    // French has many for millions; without a many branch other is used
    CHECK(format(Language::French, "{n, plural, one {a} other {b}}", {{"n", 1'000'000}}) == "b");
}

TEST_CASE("formatMessage: plurals nest and arguments work inside a branch")
{
    constexpr std::string_view pattern =
        "{a, plural, one {# file} other {# files}} in {b, plural, one {# folder} other {# folders}}";
    CHECK(format(Language::English, pattern, {{"a", 1}, {"b", 2}}) == "1 file in 2 folders");
    CHECK(format(Language::English, "{n, plural, other {{name}: #}}", {{"n", 4}, {"name", "x"}}) == "x: 4");
}

TEST_CASE("formatMessage: apostrophes are literal unless they quote braces")
{
    CHECK(format(Language::French, "l'année d'abord") == "l'année d'abord");
    CHECK(format(Language::English, "it''s") == "it's");
    CHECK(format(Language::English, "'{'not an argument'}'") == "{not an argument}");
    CHECK(format(Language::English, "a '{x}' b {x}", {{"x", 1}}) == "a {x} b 1");
}

TEST_CASE("formatMessage: mistakes are errors that say what is wrong")
{
    CHECK(formatError("{missing}").find("missing") != std::string::npos);
    CHECK(formatError("{n").find("}") != std::string::npos);
    CHECK(formatError("oops }").find("}") != std::string::npos);
    CHECK(formatError("{n, plural, one {x}}", {{"n", 2}}).find("other") != std::string::npos);
    CHECK(formatError("{n, plural, other {x}}", {{"n", "text"}}).find("number") != std::string::npos);
    CHECK(formatError("{n, select, other {x}}", {{"n", 1}}).find("plural") != std::string::npos);
}

TEST_CASE("Translator: looks up the language, falls back to English, then to the key")
{
    Translator translator;
    CHECK(translator.language() == Language::English);
    CHECK(translator.tr("menu.database") == "Database");
    CHECK(translator.tr("tracks.count", {{"count", 1}}) == "1 track");

    translator.setLanguage(Language::German);
    CHECK(translator.tr("menu.database") == "Datenbank");
    CHECK(translator.tr("menu.view") == "Ansicht");

    translator.setLanguage(Language::Russian);
    CHECK(translator.tr("tracks.count", {{"count", 3}}) == "3 трека");
    CHECK(translator.tr("tracks.count", {{"count", 12}}) == "12 треков");

    CHECK(translator.tr("no.such.key") == "no.such.key");
    CHECK(translator.tr("tracks.count") == "tracks.count"); // the argument is missing: the key shows the mistake
}

TEST_CASE("Translator: a new language is used at once")
{
    const Translator german{Language::German};
    CHECK(german.tr("library.missing", {{"count", 3}}) == "Fehlend: 3");
    CHECK(Translator{Language::Spanish}.tr("library.missing", {{"count", 3}}) == "Faltan: 3");
    CHECK(Translator{Language::French}.tr("library.missing", {{"count", 3}}) == "Manquants : 3");
}

namespace
{
    std::vector<std::string_view> keysOf(Language language)
    {
        std::vector<std::string_view> keys;
        for (const CatalogEntry &entry : catalog(language))
        {
            keys.push_back(entry.key);
        }
        return keys;
    }

    /// The names of the arguments in a message: the words in { } before a comma or the closing brace
    std::set<std::string> argumentsOf(std::string_view message)
    {
        std::set<std::string> names;
        for (std::size_t index = 0; index < message.size(); ++index)
        {
            if (message[index] == '{')
            {
                const std::size_t end = message.find_first_of(",}{", index + 1);
                const std::string name{message.substr(index + 1, end - index - 1)};
                // Branch bodies of plurals (e.g. "# tracks") have spaces and are no names
                if (!name.empty() && name.find(' ') == std::string::npos)
                {
                    names.insert(name);
                }
            }
        }
        return names;
    }
} // namespace

TEST_CASE("catalogs: every language has the keys of English, once, and no others")
{
    const auto english = keysOf(Language::English);
    CHECK(std::set<std::string_view>(english.begin(), english.end()).size() == english.size());
    for (const Language language : languages)
    {
        CAPTURE(languageCode(language));
        auto keys = keysOf(language);
        CHECK(keys.size() == english.size());
        CHECK(std::set<std::string_view>(keys.begin(), keys.end()) ==
              std::set<std::string_view>(english.begin(), english.end()));
        for (const CatalogEntry &entry : catalog(language))
        {
            CAPTURE(entry.key);
            CHECK_FALSE(entry.message.empty());
        }
    }
}

TEST_CASE("catalogs: every message formats and takes the arguments of the English one")
{
    const Translator english;
    for (const Language language : languages)
    {
        CAPTURE(languageCode(language));
        for (const CatalogEntry &entry : catalog(Language::English))
        {
            CAPTURE(entry.key);
            std::string_view translated;
            for (const CatalogEntry &candidate : catalog(language))
            {
                if (candidate.key == entry.key)
                {
                    translated = candidate.message;
                }
            }
            REQUIRE_FALSE(translated.empty());
            CHECK(argumentsOf(translated) == argumentsOf(entry.message));

            // Formats with every argument given, for all plural forms
            for (const long long n : {0LL, 1LL, 2LL, 3LL, 5LL, 11LL, 21LL, 1'000'000LL})
            {
                const std::vector<MessageArg> args{{"count", n},   {"number", n}, {"files", "files"},
                                                   {"tracks", "tracks"}, {"name", "name"}, {"not_found", n},
                                                   {"error", "error"}};
                const auto                    done = formatMessage(language, translated, args);
                CHECK_MESSAGE(done.has_value(), (done ? "" : done.error()));
            }
        }
    }
}

TEST_CASE("catalogs: plural messages have the forms their language needs")
{
    for (const Language language : {Language::Russian})
    {
        for (const std::string_view key : {"tracks.count", "files.count"})
        {
            const std::string pattern{Translator{language}.pattern(key)};
            for (const std::string_view form : {"one {", "few {", "many {", "other {"})
            {
                CHECK(pattern.find(form) != std::string::npos);
            }
        }
    }
    for (const Language language : {Language::French, Language::Spanish})
    {
        CHECK(std::string{Translator{language}.pattern("tracks.count")}.find("many {") != std::string::npos);
    }
}
