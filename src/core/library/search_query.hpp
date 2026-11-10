// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_LIBRARY_SEARCH_QUERY_HPP
#define DJW_CORE_LIBRARY_SEARCH_QUERY_HPP

#include "api/library.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace djw::core::library
{
    /// True if text holds the wildcard * or ?
    bool hasWildcard(std::string_view text);

    /**
     * Turns the words of what the user typed into an FTS5 query: every word without wildcards becomes a quoted
     * prefix term ("word"*), all of them must match. Quotes in the input are escaped, so no input can break the
     * query syntax. No such words give an empty query, which means: no filter.
     */
    std::string ftsQuery(std::string_view text);

    /**
     * A GLOB pattern for a folded column value (see foldText): with * or ? the pattern must match the whole value,
     * without them it matches when the value contains the text. GLOB's own syntax ([...]) is escaped.
     */
    std::string globPattern(std::string_view filter);

    /// The same, but matching anywhere in the value even with wildcards, for words of the search text
    std::string containsPattern(std::string_view word);

    /// SQL for a library search and its text parameters, numbered from 1 in order
    struct SearchSql
    {
            std::string              sql;
            std::vector<std::string> parameters;
    };

    /**
     * The search as one SELECT over the tracks table (alias t) returning columns: FTS5 for the plain words of the
     * text, GLOB on the folded key columns for words with wildcards and for the column filters, sorted with empty
     * values last.
     */
    SearchSql buildSearch(const api::LibraryQuery &query, std::string_view columns);
} // namespace djw::core::library

#endif // DJW_CORE_LIBRARY_SEARCH_QUERY_HPP
