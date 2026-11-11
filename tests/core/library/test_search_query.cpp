// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library/search_query.hpp"

#include <doctest/doctest.h>

using namespace djw::core::library;
namespace api = djw::core::api;

TEST_CASE("ftsQuery: every word becomes a prefix term")
{
    CHECK(ftsQuery("daft") == R"("daft"*)");
    CHECK(ftsQuery("daft one") == R"("daft"* "one"*)");
}

TEST_CASE("ftsQuery: blank text is no filter, extra whitespace is ignored")
{
    CHECK(ftsQuery("").empty());
    CHECK(ftsQuery("  \t ").empty());
    CHECK(ftsQuery("  daft \t one  ") == R"("daft"* "one"*)");
    CHECK(ftsQuery("daft\r\none\rmore") == R"("daft"* "one"* "more"*)"); // line ends separate words too
}

TEST_CASE("ftsQuery: FTS5 syntax in the input stays plain text")
{
    CHECK(ftsQuery(R"(say "hi")") == R"("say"* """hi"""*)");
    CHECK(ftsQuery("a OR b") == R"("a"* "OR"* "b"*)");
    CHECK(ftsQuery("title:x -y (z)") == R"q("title:x"* "-y"* "(z)"*)q");
}

TEST_CASE("ftsQuery: words with wildcards are left to the GLOB conditions")
{
    CHECK(ftsQuery("daft p?nk one*") == R"("daft"*)");
    CHECK(ftsQuery("*").empty());
}

TEST_CASE("globPattern: contains without wildcards, the whole value with them, folded")
{
    CHECK(globPattern("Daft") == "*daft*");
    CHECK(globPattern("Daft*") == "daft*");
    CHECK(globPattern("*PUNK") == "*punk");
    CHECK(globPattern("d?ft punk") == "d?ft punk");
    CHECK(globPattern("Ärzte") == "*arzte*");
}

TEST_CASE("globPattern: GLOB character classes in the input are plain characters")
{
    CHECK(globPattern("[remix]") == "*[[]remix]*");
}

TEST_CASE("containsPattern: matches anywhere, also with wildcards")
{
    CHECK(containsPattern("d?ft") == "*d?ft*");
}

TEST_CASE("buildSearch: no conditions for an empty query, sorted by artist with empty values last")
{
    const SearchSql search = buildSearch({.limit = 50}, "t.id");
    CHECK(search.sql ==
          "SELECT t.id FROM tracks t ORDER BY t.artist_key = '', t.artist_key, t.title_key, t.id LIMIT 50");
    CHECK(search.parameters.empty());
}

TEST_CASE("buildSearch: every part of the query becomes one condition with its parameter, in order")
{
    const SearchSql search = buildSearch({.text       = "daft w?rld",
                                          .artist     = "Daft",
                                          .title      = "Around*",
                                          .genre      = "house",
                                          .year       = "199?",
                                          .sort_by    = api::LibraryColumn::Year,
                                          .descending = true},
                                         "t.id");
    CHECK(search.parameters ==
          std::vector<std::string>{R"("daft"*)", "*w?rld*", "*daft*", "around*", "*house*", "199?"});
    CHECK(search.sql.find("tracks_search MATCH ?") != std::string::npos);
    CHECK(search.sql.find("t.any_key GLOB ?") != std::string::npos);
    CHECK(search.sql.find("t.artist_key GLOB ?") != std::string::npos);
    CHECK(search.sql.find("t.title_key GLOB ?") != std::string::npos);
    CHECK(search.sql.find("t.genre_key GLOB ?") != std::string::npos);
    CHECK(search.sql.find("CAST(t.year AS TEXT) GLOB ?") != std::string::npos);
    CHECK(search.sql.find("ORDER BY t.year = 0, t.year DESC, t.artist_key, t.title_key, t.id") != std::string::npos);
}
