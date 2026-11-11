// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library/track_store.hpp"

#include "library_downgrade.hpp"
#include "temp_dir.hpp"

#include <doctest/doctest.h>

using djw::core::library::FileStamp;
namespace api = djw::core::api;
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
                return titles(api::LibraryQuery{.text = std::string{typed}, .limit = limit});
            }

            std::vector<std::string> titles(const api::LibraryQuery &query)
            {
                const auto found = store.search(query);
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

    const auto found = fixture.store.search({});
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

    const auto found = second.search({});
    REQUIRE(found.has_value());
    REQUIRE(found->size() == 1);
    CHECK(found->front().title == "Kept");
}

TEST_CASE("TrackStore: non-ASCII paths are stored as they are")
{
    Fixture fixture;
    fixture.add("Ärger über Öl – ½.mp3", {.title = "T"});

    const auto found = fixture.store.search({.text = "ärger"});
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

TEST_CASE("TrackStore: column filters match contained text, or with wildcards the whole value, without case or accents")
{
    Fixture fixture;
    fixture.add("1.mp3", {.artist = "Daft Punk", .title = "One More Time", .genre = "House", .year = 2000});
    fixture.add("2.mp3", {.artist = "Punk Daft", .title = "Around the World", .genre = "Deep House", .year = 1997});
    fixture.add("3.mp3", {.artist = "Die Ärzte", .title = "Schrei nach Liebe", .genre = "Punk", .year = 1993});

    CHECK(fixture.titles({.artist = "punk"}).size() == 2);
    CHECK(fixture.titles({.artist = "daft*"}) == std::vector<std::string>{"One More Time"});
    CHECK(fixture.titles({.artist = "*ARZTE"}) == std::vector<std::string>{"Schrei nach Liebe"});
    CHECK(fixture.titles({.title = "around"}) == std::vector<std::string>{"Around the World"});
    CHECK(fixture.titles({.genre = "house"}).size() == 2);
    CHECK(fixture.titles({.genre = "h?use"}) == std::vector<std::string>{"One More Time"});
    CHECK(fixture.titles({.year = "199?"}).size() == 2);
    CHECK(fixture.titles({.year = "2000"}) == std::vector<std::string>{"One More Time"});
    CHECK(fixture.titles({.artist = "daft", .genre = "deep"}) == std::vector<std::string>{"Around the World"});
}

TEST_CASE("TrackStore: a year filter never matches tracks without a year")
{
    Fixture fixture;
    fixture.add("1.mp3", {.title = "No Year"});
    CHECK(fixture.titles({.year = "*"}).empty());
    CHECK(fixture.titles({.year = "0"}).empty());
}

TEST_CASE("TrackStore: search words with wildcards match anywhere, together with the other words")
{
    Fixture fixture;
    fixture.add("Club Mix.mp3", {.artist = "Daft Punk", .title = "One More Time"});
    fixture.add("2.mp3", {.artist = "Daft Punk", .title = "Around the World"});

    CHECK(fixture.titles("w?rld") == std::vector<std::string>{"Around the World"});
    CHECK(fixture.titles("daft *ore") == std::vector<std::string>{"One More Time"});
    CHECK(fixture.titles("cl*mix") == std::vector<std::string>{"One More Time"}); // file name
}

TEST_CASE("TrackStore: sorts by any column in both directions, empty values last, ties by artist and title")
{
    Fixture fixture;
    fixture.add("1.mp3", {.artist = "b", .title = "y", .genre = "Rock", .year = 1990, .duration_seconds = 300});
    fixture.add("2.mp3", {.artist = "Ä", .title = "x", .genre = "", .year = 0, .duration_seconds = 100});
    fixture.add("3.mp3", {.artist = "", .title = "z", .genre = "House", .year = 2010, .duration_seconds = 200});
    fixture.add("4.mp3", {.artist = "c", .title = "", .genre = "house", .year = 1990, .duration_seconds = 0});

    using C           = api::LibraryColumn;
    const auto titles = [&](C column, bool descending)
    { return fixture.titles(api::LibraryQuery{.sort_by = column, .descending = descending}); };

    CHECK(titles(C::Artist, false) == std::vector<std::string>{"x", "y", "", "z"}); // Ä sorts as a
    CHECK(titles(C::Artist, true) == std::vector<std::string>{"", "y", "x", "z"});
    CHECK(titles(C::Title, false) == std::vector<std::string>{"x", "y", "z", ""});
    CHECK(titles(C::Title, true) == std::vector<std::string>{"z", "y", "x", ""});
    // Both house: the tie goes by artist, where empty counts as smallest; empty last holds for the sort column
    CHECK(titles(C::Genre, false) == std::vector<std::string>{"z", "", "y", "x"});
    CHECK(titles(C::Year, false) == std::vector<std::string>{"y", "", "z", "x"}); // 1990: b, c
    CHECK(titles(C::Year, true) == std::vector<std::string>{"z", "y", "", "x"});
    CHECK(titles(C::Duration, false) == std::vector<std::string>{"x", "z", "y", ""});
    CHECK(titles(C::Duration, true) == std::vector<std::string>{"y", "z", "x", ""});
}

TEST_CASE("TrackStore: a version 1 database is migrated, its tracks get their keys")
{
    Fixture fixture;
    fixture.add("Some File.mp3", {.artist = "Die Ärzte", .title = "Westerland", .genre = "Punk"});
    djw::tests::downgradeLibrary(fixture.database(), 1);

    auto migrated = TrackStore::open(fixture.database());
    REQUIRE_MESSAGE(migrated.has_value(), migrated.error());
    fixture.store = std::move(*migrated);

    CHECK(fixture.titles({.artist = "arzte"}) == std::vector<std::string>{"Westerland"});
    CHECK(fixture.titles("so?e") == std::vector<std::string>{"Westerland"});
    CHECK(fixture.titles({.genre = "punk"}) == std::vector<std::string>{"Westerland"});
}

TEST_CASE("TrackStore: a version 2 database gets the missing flag, all tracks start as present")
{
    Fixture fixture;
    fixture.add("a.mp3", {.title = "Kept"});
    djw::tests::downgradeLibrary(fixture.database(), 2);

    auto migrated = TrackStore::open(fixture.database());
    REQUIRE_MESSAGE(migrated.has_value(), migrated.error());
    fixture.store = std::move(*migrated);

    CHECK(fixture.titles("kept") == std::vector<std::string>{"Kept"});
    CHECK(fixture.store.missingCount().value() == 0);
    CHECK(fixture.titles({.only_missing = true}).empty());
}

TEST_CASE("TrackStore: a track has no cue point at first, a stored one comes back, null removes it")
{
    Fixture fixture;
    fixture.add("a.mp3", {.title = "a"});
    fixture.add("b.mp3", {.title = "b"});
    auto tracks = fixture.store.search({.sort_by = api::LibraryColumn::Title}).value();
    REQUIRE(tracks.size() == 2);
    CHECK_FALSE(tracks[0].cue_seconds.has_value());

    REQUIRE(fixture.store.setCue(tracks[0].id, 12.345).has_value());
    tracks = fixture.store.search({.sort_by = api::LibraryColumn::Title}).value();
    REQUIRE(tracks[0].cue_seconds.has_value());
    CHECK(*tracks[0].cue_seconds == doctest::Approx(12.345));
    CHECK_FALSE(tracks[1].cue_seconds.has_value()); // only that track

    REQUIRE(fixture.store.setCue(tracks[0].id, 0.0).has_value()); // 0 is a cue point too, not none
    CHECK(fixture.store.search({.sort_by = api::LibraryColumn::Title}).value()[0].cue_seconds == 0.0);

    REQUIRE(fixture.store.setCue(tracks[0].id, std::nullopt).has_value());
    CHECK_FALSE(fixture.store.search({.sort_by = api::LibraryColumn::Title}).value()[0].cue_seconds.has_value());
}

TEST_CASE("TrackStore: the cue point survives a rescan of the file")
{
    Fixture fixture;
    fixture.add("a.mp3", {.title = "a"});
    const auto id = fixture.store.search({}).value().front().id;
    REQUIRE(fixture.store.setCue(id, 31.5).has_value());

    fixture.add("a.mp3", {.title = "a, retagged"}); // the scan stores the file again
    const auto found = fixture.store.search({}).value();
    REQUIRE(found.size() == 1);
    CHECK(found.front().title == "a, retagged");
    REQUIRE(found.front().cue_seconds.has_value());
    CHECK(*found.front().cue_seconds == doctest::Approx(31.5));
}

TEST_CASE("TrackStore: a version 4 database gets the cue column, its tracks have no cue point")
{
    Fixture fixture;
    fixture.add("a.mp3", {.title = "Kept"});
    djw::tests::downgradeLibrary(fixture.database(), 4);

    auto migrated = TrackStore::open(fixture.database());
    REQUIRE_MESSAGE(migrated.has_value(), migrated.error());
    fixture.store = std::move(*migrated);

    const auto found = fixture.store.search({}).value();
    REQUIRE(found.size() == 1);
    CHECK_FALSE(found.front().cue_seconds.has_value());
    REQUIRE(fixture.store.setCue(found.front().id, 5.0).has_value());
    CHECK(fixture.store.search({}).value().front().cue_seconds == 5.0);
}

TEST_CASE("TrackStore: a track is not analyzed at first, the analysis comes back with tempo and waveform")
{
    Fixture fixture;
    fixture.add("a.mp3", {.title = "a"});
    fixture.add("b.mp3", {.title = "b"});
    auto tracks = fixture.store.search({.sort_by = api::LibraryColumn::Title}).value();
    CHECK_FALSE(tracks[0].analyzed);
    CHECK_FALSE(tracks[0].bpm.has_value());
    CHECK(fixture.store.waveform(tracks[0].id).value().empty());
    CHECK(fixture.store.unanalyzed().value().size() == 2);

    const std::vector<std::uint8_t> waveform{0, 10, 255, 3};
    REQUIRE(fixture.store.setAnalysis(tracks[0].id, 127.5, waveform).has_value());

    tracks = fixture.store.search({.sort_by = api::LibraryColumn::Title}).value();
    CHECK(tracks[0].analyzed);
    REQUIRE(tracks[0].bpm.has_value());
    CHECK(*tracks[0].bpm == doctest::Approx(127.5));
    CHECK_FALSE(tracks[1].analyzed); // only that track
    CHECK(fixture.store.waveform(tracks[0].id).value() == waveform);
    CHECK(fixture.store.isAnalyzed(tracks[0].id).value());
    CHECK_FALSE(fixture.store.isAnalyzed(tracks[1].id).value());
    CHECK(fixture.store.unanalyzed().value().size() == 1);

    // No tempo found is a result too: analyzed, without tempo and with an empty waveform
    REQUIRE(fixture.store.setAnalysis(tracks[1].id, std::nullopt, {}).has_value());
    tracks = fixture.store.search({.sort_by = api::LibraryColumn::Title}).value();
    CHECK(tracks[1].analyzed);
    CHECK_FALSE(tracks[1].bpm.has_value());
    CHECK(fixture.store.unanalyzed().value().empty());
}

TEST_CASE("TrackStore: a changed file loses its analysis, a tag edit keeps it")
{
    Fixture fixture;
    fixture.add("a.mp3", {.title = "a"});
    const auto id = fixture.store.search({}).value().front().id;
    REQUIRE(fixture.store.setAnalysis(id, 120.0, std::vector<std::uint8_t>{1, 2, 3}).has_value());

    REQUIRE(fixture.store.store(fixture.folder.path() / "a.mp3", {.modified = 1, .size = 2}, {.title = "a, retagged"}, true)
                .has_value());
    auto found = fixture.store.search({}).value();
    CHECK(found.front().title == "a, retagged");
    CHECK(found.front().analyzed);
    CHECK(found.front().bpm == 120.0);
    CHECK(fixture.store.waveform(id).value().size() == 3);

    fixture.add("a.mp3", {.title = "a, other file"}); // the scan found a changed file
    found = fixture.store.search({}).value();
    CHECK_FALSE(found.front().analyzed);
    CHECK_FALSE(found.front().bpm.has_value());
    CHECK(fixture.store.waveform(id).value().empty());
}

TEST_CASE("TrackStore: a version 5 database gets the analysis columns, its tracks are not analyzed")
{
    Fixture fixture;
    fixture.add("a.mp3", {.title = "Kept"});
    djw::tests::downgradeLibrary(fixture.database(), 5);

    auto migrated = TrackStore::open(fixture.database());
    REQUIRE_MESSAGE(migrated.has_value(), migrated.error());
    fixture.store = std::move(*migrated);

    const auto tracks = fixture.store.search({}).value();
    REQUIRE(tracks.size() == 1);
    CHECK(tracks.front().title == "Kept");
    CHECK_FALSE(tracks.front().analyzed);
    CHECK_FALSE(tracks.front().bpm.has_value());
    CHECK(fixture.store.unanalyzed().value().size() == 1);
}

TEST_CASE("TrackStore: sorting by tempo puts tracks without one last in both directions, a track comes back by its id")
{
    Fixture fixture;
    fixture.add("a.mp3", {.title = "a"});
    fixture.add("b.mp3", {.title = "b"});
    fixture.add("c.mp3", {.title = "c"});
    auto tracks = fixture.store.search({.sort_by = api::LibraryColumn::Title}).value();
    REQUIRE(fixture.store.setAnalysis(tracks[0].id, 140.0, std::vector<std::uint8_t>{1}).has_value());
    REQUIRE(fixture.store.setAnalysis(tracks[2].id, 90.0, std::vector<std::uint8_t>{1}).has_value());

    CHECK(fixture.titles({.sort_by = api::LibraryColumn::Bpm}) == std::vector<std::string>{"c", "a", "b"});
    CHECK(fixture.titles({.sort_by = api::LibraryColumn::Bpm, .descending = true}) ==
          std::vector<std::string>{"a", "c", "b"});

    const auto found = fixture.store.track(tracks[0].id);
    REQUIRE(found.has_value());
    REQUIRE(found->has_value());
    CHECK((*found)->title == "a");
    CHECK((*found)->bpm == 140.0);
    CHECK_FALSE(fixture.store.track(4711).value().has_value());
}
