// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_LIBRARY_SEARCH_QUERY_HPP
#define DJW_CORE_LIBRARY_SEARCH_QUERY_HPP

#include <string>
#include <string_view>

namespace djw::core::library
{
    /**
     * Turns what the user typed into an FTS5 query: every word becomes a quoted prefix term ("word"*), all of them
     * must match. Quotes in the input are escaped, so no input can break the query syntax. Empty or blank text gives
     * an empty query, which means: no filter.
     */
    std::string ftsQuery(std::string_view text);
} // namespace djw::core::library

#endif // DJW_CORE_LIBRARY_SEARCH_QUERY_HPP
