// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library/search_query.hpp"
#include "library/track_store.hpp"

#include "temp_dir.hpp"

#include <doctest/doctest.h>

using djw::core::library::FileStamp;
using djw::core::library::ftsQuery;
using djw::core::library::TrackStore;
using djw::core::library::TrackTags;

namespace
{
    struct Fixture
    {
            Fixture() : store{open()} {}

            [[nodiscard]] TrackStore open() const
            {
                auto opened = TrackStore::open(database());
                REQUIRE_MESSAGE(opened.has_value(), opened.error());
                return std::move(*opened);
            }

            [[nodiscard]] std::filesystem::path database() const { return folder.path() / "library.db"; }

            void add(const std::string &file, const TrackTags &tags)
            {
                REQUIRE(store.store(folder.path() / file, {.modified = 1, .size = 2}, tags).has_value());
            }

            /// Titles found for what the user typed
            std::vector<std::string> titles(std::string_view typed, std::size_t limit = 100)
            {
                const auto found = store.search(ftsQuery(typed), limit);
                REQUIRE_MESSAGE(found.has_value(), found.error());
                std::vector<std::string> result;
                for (const auto &track : *found)
                {
                    result.push_back(track.title);
                }
                return result;
            }

            djw::tests::TempDir folder;
            TrackStore          store;
    };
} // namespace

TEST_CASE("TrackStore: a stored track comes back with all its fields")
{
    Fixture fixture;
    fixture.add("a.mp3", {.artist           = "Artist",
                          .title            = "Title",
                          .album            = "Album",
                          .genre            = "Genre",
                          .year             = 2001,
                          .duration_seconds = 245.5});

    const auto found = fixture.store.search("", 10);
    REQUIRE(found.has_value());
    REQUIRE(found->size() == 1);
    const auto &track = found->front();
    CHECK(track.id > 0);
    CHECK(track.path == fixture.folder.path() / "a.mp3");
    CHECK(track.artist == "Artist");
    CHECK(track.title == "Title");
    CHECK(track.album == "Album");
    CHECK(track.genre == "Genre");
    CHECK(track.year == 2001);
    CHECK(track.duration_seconds == 245.5);
    CHECK(fixture.store.trackCount().value() == 1);
}

TEST_CASE("TrackStore: storing a path again updates the track and its search entry")
{
    Fixture fixture;
    fixture.add("a.mp3", {.artist = "Old", .title = "Old Title"});
    fixture.add("a.mp3", {.artist = "New", .title = "New Title"});

    CHECK(fixture.store.trackCount().value() == 1);
    CHECK(fixture.titles("old").empty());
    CHECK(fixture.titles("new") == std::vector<std::string>{"New Title"});
}

TEST_CASE("TrackStore: the stamp tells whether a file changed")
{
    Fixture    fixture;
    const auto file = fixture.folder.path() / "a.mp3";
    CHECK(fixture.store.stamp(file).value() == std::optional<FileStamp>{});

    REQUIRE(fixture.store.store(file, {.modified = 42, .size = 1'000}, {}).has_value());

    CHECK(fixture.store.stamp(file).value() == std::optional<FileStamp>{FileStamp{.modified = 42, .size = 1'000}});
}

TEST_CASE("TrackStore: search matches word starts in every field, all words must match")
{
    Fixture fixture;
    fixture.add("Club Mix.mp3", {.artist = "Daft Punk", .title = "One More Time", .genre = "House"});
    fixture.add("b.mp3", {.artist = "Daft Punk", .title = "Around the World", .album = "Homework"});
    fixture.add("c.mp3", {.artist = "Moby", .title = "Porcelain", .genre = "Downtempo"});

    CHECK(fixture.titles("daf").size() == 2);
    CHECK(fixture.titles("daft one") == std::vector<std::string>{"One More Time"});
    CHECK(fixture.titles("HOMEW") == std::vector<std::string>{"Around the World"}); // album, any case
    CHECK(fixture.titles("down") == std::vector<std::string>{"Porcelain"});         // genre
    CHECK(fixture.titles("club") == std::vector<std::string>{"One More Time"});     // file name
    CHECK(fixture.titles("unk").empty());                                           // not a word start
    CHECK(fixture.titles("daft moby").empty());
}

TEST_CASE("TrackStore: search ignores accents and handles input with FTS5 syntax")
{
    Fixture fixture;
    fixture.add("a.flac", {.artist = "Beyoncé", .title = "Halo"});

    CHECK(fixture.titles("beyonce") == std::vector<std::string>{"Halo"});
    CHECK(fixture.titles("BEYONCÉ") == std::vector<std::string>{"Halo"});
    CHECK(fixture.titles(R"(" OR *)").empty());
}

TEST_CASE("TrackStore: results are ordered by artist and title, without case, and limited")
{
    Fixture fixture;
    fixture.add("1.mp3", {.artist = "b", .title = "z"});
    fixture.add("2.mp3", {.artist = "A", .title = "y"});
    fixture.add("3.mp3", {.artist = "b", .title = "X"});

    CHECK(fixture.titles("") == std::vector<std::string>{"y", "X", "z"});
    CHECK(fixture.titles("", 2) == std::vector<std::string>{"y", "X"});
}

TEST_CASE("TrackStore: folders are kept once each")
{
    Fixture fixture;
    REQUIRE(fixture.store.addFolder("/music/b").has_value());
    REQUIRE(fixture.store.addFolder("/music/a").has_value());
    REQUIRE(fixture.store.addFolder("/music/b").has_value());

    CHECK(fixture.store.folders() ==
          std::vector<std::filesystem::path>{std::filesystem::path{"/music/a"}, std::filesystem::path{"/music/b"}});
}

TEST_CASE("TrackStore: the data stays in the file, a second connection sees it")
{
    Fixture fixture;
    fixture.add("a.mp3", {.title = "Kept"});

    TrackStore second = fixture.open();

    const auto found = second.search("", 10);
    REQUIRE(found.has_value());
    REQUIRE(found->size() == 1);
    CHECK(found->front().title == "Kept");
}

TEST_CASE("TrackStore: non-ASCII paths are stored as they are")
{
    Fixture fixture;
    fixture.add("Ärger über Öl – ½.mp3", {.title = "T"});

    const auto found = fixture.store.search(ftsQuery("ärger"), 10);
    REQUIRE(found.has_value());
    REQUIRE(found->size() == 1);
    CHECK(found->front().path == fixture.folder.path() / "Ärger über Öl – ½.mp3");
}

TEST_CASE("TrackStore: a database from a newer schema version is refused")
{
    Fixture fixture;
    {
        auto database = djw::core::library::SqliteDatabase::open(fixture.database());
        REQUIRE(database.has_value());
        REQUIRE(database->execute("PRAGMA user_version = 99").has_value());
    }

    const auto opened = TrackStore::open(fixture.database());

    REQUIRE_FALSE(opened.has_value());
    CHECK(opened.error().find("schema version 99") != std::string::npos);
}

TEST_CASE("TrackStore: a database file that cannot be opened is an error")
{
    const djw::tests::TempDir folder;
    const auto                opened = TrackStore::open(folder.path() / "missing" / "library.db");
    REQUIRE_FALSE(opened.has_value());
    CHECK(opened.error().find("library.db") != std::string::npos);
}
