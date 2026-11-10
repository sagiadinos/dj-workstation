// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library/search_query.hpp"

#include "library/text_fold.hpp"

namespace djw::core::library
{
    namespace
    {
        bool isSpace(char character)
        {
            return character == ' ' || character == '\t' || character == '\n' || character == '\r';
        }

        std::vector<std::string_view> words(std::string_view text)
        {
            std::vector<std::string_view> result;
            std::size_t                   position = 0;
            while (position < text.size())
            {
                while (position < text.size() && isSpace(text[position]))
                {
                    ++position;
                }
                const std::size_t start = position;
                while (position < text.size() && !isSpace(text[position]))
                {
                    ++position;
                }
                if (position > start)
                {
                    result.push_back(text.substr(start, position - start));
                }
            }
            return result;
        }

        /// The folded text with GLOB's character classes escaped; * and ? stay wildcards
        std::string escapedFold(std::string_view text)
        {
            std::string escaped;
            for (const char character : foldText(text))
            {
                if (character == '[')
                {
                    escaped += "[[]";
                }
                else
                {
                    escaped += character;
                }
            }
            return escaped;
        }

        /// ORDER BY terms: emptiness first (empty values last in both directions), then the value itself
        std::string orderBy(api::LibraryColumn column, bool descending)
        {
            const char *direction = descending ? " DESC" : "";
            switch (column)
            {
                case api::LibraryColumn::Title:
                    return std::string{"t.title_key = '', t.title_key"} + direction + ", t.artist_key";
                case api::LibraryColumn::Genre:
                    return std::string{"t.genre_key = '', t.genre_key"} + direction + ", t.artist_key, t.title_key";
                case api::LibraryColumn::Year:
                    return std::string{"t.year = 0, t.year"} + direction + ", t.artist_key, t.title_key";
                case api::LibraryColumn::Duration:
                    return std::string{"t.duration = 0, t.duration"} + direction + ", t.artist_key, t.title_key";
                case api::LibraryColumn::Artist:
                    break;
            }
            return std::string{"t.artist_key = '', t.artist_key"} + direction + ", t.title_key";
        }
    } // namespace

    bool hasWildcard(std::string_view text)
    {
        return text.find_first_of("*?") != std::string_view::npos;
    }

    std::string ftsQuery(std::string_view text)
    {
        std::string query;
        for (const std::string_view word : words(text))
        {
            if (hasWildcard(word))
            {
                continue;
            }
            if (!query.empty())
            {
                query += ' ';
            }
            query += '"';
            for (const char character : word)
            {
                if (character == '"')
                {
                    query += '"'; // doubled inside a string
                }
                query += character;
            }
            query += "\"*";
        }
        return query;
    }

    std::string globPattern(std::string_view filter)
    {
        const std::string escaped = escapedFold(filter);
        return hasWildcard(filter) ? escaped : "*" + escaped + "*";
    }

    std::string containsPattern(std::string_view word)
    {
        return "*" + escapedFold(word) + "*";
    }

    SearchSql buildSearch(const api::LibraryQuery &query, std::string_view columns)
    {
        SearchSql                search;
        std::vector<std::string> conditions;
        const auto               add = [&](std::string condition, std::string parameter)
        {
            conditions.push_back(std::move(condition));
            search.parameters.push_back(std::move(parameter));
        };

        if (std::string fts = ftsQuery(query.text); !fts.empty())
        {
            add("t.id IN (SELECT rowid FROM tracks_search WHERE tracks_search MATCH ?)", std::move(fts));
        }
        for (const std::string_view word : words(query.text))
        {
            if (hasWildcard(word))
            {
                add("t.any_key GLOB ?", containsPattern(word));
            }
        }
        if (!query.artist.empty())
        {
            add("t.artist_key GLOB ?", globPattern(query.artist));
        }
        if (!query.title.empty())
        {
            add("t.title_key GLOB ?", globPattern(query.title));
        }
        if (!query.genre.empty())
        {
            add("t.genre_key GLOB ?", globPattern(query.genre));
        }
        if (!query.year.empty())
        {
            add("t.year != 0 AND CAST(t.year AS TEXT) GLOB ?", globPattern(query.year));
        }

        search.sql = "SELECT " + std::string{columns} + " FROM tracks t";
        for (std::size_t index = 0; index < conditions.size(); ++index)
        {
            search.sql += (index == 0 ? " WHERE " : " AND ") + conditions[index];
        }
        search.sql +=
            " ORDER BY " + orderBy(query.sort_by, query.descending) + ", t.id LIMIT " + std::to_string(query.limit);
        return search;
    }
} // namespace djw::core::library
