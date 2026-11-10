// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library/search_query.hpp"

namespace djw::core::library
{
    namespace
    {
        bool isSpace(char character)
        {
            return character == ' ' || character == '\t' || character == '\n' || character == '\r';
        }
    } // namespace

    std::string ftsQuery(std::string_view text)
    {
        std::string query;
        std::size_t position = 0;
        while (position < text.size())
        {
            while (position < text.size() && isSpace(text[position]))
            {
                ++position;
            }
            if (position == text.size())
            {
                break;
            }
            if (!query.empty())
            {
                query += ' ';
            }
            query += '"';
            for (; position < text.size() && !isSpace(text[position]); ++position)
            {
                if (text[position] == '"')
                {
                    query += '"'; // doubled inside a string
                }
                query += text[position];
            }
            query += "\"*";
        }
        return query;
    }
} // namespace djw::core::library
