// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library/file_name_tags.hpp"

#include <doctest/doctest.h>

using djw::core::library::tagsFromFileName;
using djw::core::library::TrackTags;
using djw::core::library::withFileNameFallback;

TEST_CASE("tagsFromFileName: \"Artist - Title\" gives artist and title")
{
    const TrackTags tags = tagsFromFileName("/music/house/Daft Punk - One More Time.mp3");
    CHECK(tags.artist == "Daft Punk");
    CHECK(tags.title == "One More Time");
}

TEST_CASE("tagsFromFileName: splits at the first separator, the rest belongs to the title")
{
    const TrackTags tags = tagsFromFileName("Artist - Title - Radio Edit.flac");
    CHECK(tags.artist == "Artist");
    CHECK(tags.title == "Title - Radio Edit");
}

TEST_CASE("tagsFromFileName: without a separator the whole name is the title")
{
    const TrackTags tags = tagsFromFileName("track01.wav");
    CHECK(tags.artist.empty());
    CHECK(tags.title == "track01");
}

TEST_CASE("tagsFromFileName: underscores count as spaces, surrounding spaces go")
{
    const TrackTags tags = tagsFromFileName("Some_Artist_-_Some_Title_.mp3");
    CHECK(tags.artist == "Some Artist");
    CHECK(tags.title == "Some Title");
}

TEST_CASE("tagsFromFileName: a hyphen without spaces is part of the name")
{
    CHECK(tagsFromFileName("Jay-Z.mp3").title == "Jay-Z");
}

TEST_CASE("withFileNameFallback: fills only what the tags lack")
{
    const std::filesystem::path file{"Name Artist - Name Title.mp3"};

    const TrackTags complete = withFileNameFallback({.artist = "A", .title = "T", .genre = "G"}, file);
    CHECK(complete == TrackTags{.artist = "A", .title = "T", .genre = "G"});

    const TrackTags no_title = withFileNameFallback({.artist = "A", .year = 1999}, file);
    CHECK(no_title == TrackTags{.artist = "A", .title = "Name Title", .year = 1999});

    const TrackTags no_artist = withFileNameFallback({.title = "T"}, file);
    CHECK(no_artist == TrackTags{.artist = "Name Artist", .title = "T"});

    const TrackTags nothing = withFileNameFallback({}, file);
    CHECK(nothing == TrackTags{.artist = "Name Artist", .title = "Name Title"});
}

TEST_CASE("tagsFromFileName: spaces before and after a name without separator go as well")
{
    CHECK(tagsFromFileName("  track01  .mp3").title == "track01");
    CHECK(tagsFromFileName("   .mp3").title.empty());
}
