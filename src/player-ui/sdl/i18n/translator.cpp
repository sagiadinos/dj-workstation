// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "i18n/translator.hpp"

#include "i18n/catalog.hpp"

#include <span>

namespace djw::player_ui::sdl
{
    namespace
    {
        std::unordered_map<std::string_view, std::string_view> indexOf(std::span<const CatalogEntry> entries)
        {
            std::unordered_map<std::string_view, std::string_view> index;
            index.reserve(entries.size());
            for (const CatalogEntry &entry : entries)
            {
                index.emplace(entry.key, entry.message);
            }
            return index;
        }
    } // namespace

    Translator::Translator(Language language)
        : current{language}, texts{indexOf(catalog(language))}, fallback{indexOf(catalog(Language::English))}
    {
    }

    void Translator::setLanguage(Language language)
    {
        if (language == current)
        {
            return;
        }
        current = language;
        texts   = indexOf(catalog(language));
    }

    std::string_view Translator::pattern(std::string_view key) const
    {
        if (const auto found = texts.find(key); found != texts.end())
        {
            return found->second;
        }
        if (const auto found = fallback.find(key); found != fallback.end())
        {
            return found->second;
        }
        return {};
    }

    std::string Translator::tr(std::string_view key, std::initializer_list<MessageArg> args) const
    {
        const std::string_view message = pattern(key);
        if (message.empty())
        {
            return std::string{key};
        }
        auto filled = formatMessage(current, message, std::span<const MessageArg>{args.begin(), args.size()});
        return filled ? std::move(*filled) : std::string{key};
    }
} // namespace djw::player_ui::sdl
