// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library/search_query.hpp"

#include <doctest/doctest.h>

using djw::core::library::ftsQuery;

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
}

TEST_CASE("ftsQuery: FTS5 syntax in the input stays plain text")
{
    CHECK(ftsQuery(R"(say "hi")") == R"("say"* """hi"""*)");
    CHECK(ftsQuery("a OR b") == R"("a"* "OR"* "b"*)");
    CHECK(ftsQuery("title:x -y (z) *") == R"q("title:x"* "-y"* "(z)"* "*"*)q");
}
