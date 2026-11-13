// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library/missing_files.hpp"
#include "library/playlist_store.hpp"
#include "library/track_store.hpp"

#include "library_downgrade.hpp"
#include "sql_fault.hpp"
#include "temp_dir.hpp"

#include <doctest/doctest.h>

#include <fstream>

using namespace djw::core::library;
namespace api = djw::core::api;

namespace
{
    struct Fixture
    {
            Fixture() : tracks{TrackStore::open(data.path() / "library.db").value()}
            {
                playlists.emplace(PlaylistStore::open(tracks.connection()).value());
            }

            std::int64_t addTrack(const std::string &title)
            {
                const auto file = data.path() / (title + ".mp3");
                REQUIRE(tracks.store(file, {.modified = 1, .size = 1}, {.title = title}).has_value());
                return tracks.trackId(file).value().value();
            }

            std::vector<std::string> titles(std::int64_t playlist)
            {
                std::vector<std::string> result;
                const auto               found = playlists->tracks(playlist).value();
                for (const auto &track : found)
                {
                    result.push_back(track.title);
                }
                return result;
            }

            djw::tests::TempDir          data;
            TrackStore                   tracks;
            std::optional<PlaylistStore> playlists;
    };
} // namespace

TEST_CASE("PlaylistStore: playlists are created, listed by name with their track count, renamed and removed")
{
    Fixture    fixture;
    const auto house   = fixture.playlists->create("House").value();
    const auto ambient = fixture.playlists->create("ambient").value();
    REQUIRE(fixture.playlists->setEntries(house, {fixture.addTrack("a"), fixture.addTrack("b")}).has_value());

    auto list = fixture.playlists->list().value();
    REQUIRE(list.size() == 2);
    CHECK(list[0].name == "ambient");
    CHECK(list[0].track_count == 0);
    CHECK(list[1].name == "House");
    CHECK(list[1].track_count == 2);
    CHECK(fixture.playlists->idOf("House").value() == house);
    CHECK_FALSE(fixture.playlists->idOf("Techno").value().has_value());

    REQUIRE(fixture.playlists->rename(ambient, "Chill").has_value());
    REQUIRE(fixture.playlists->remove(house).has_value());
    list = fixture.playlists->list().value();
    REQUIRE(list.size() == 1);
    CHECK(list[0].name == "Chill");
    CHECK(fixture.playlists->entries(house).value().empty()); // the entries went with it
}

TEST_CASE("PlaylistStore: entries keep their order, the same track may appear twice")
{
    Fixture    fixture;
    const auto list = fixture.playlists->create("Set").value();
    const auto a    = fixture.addTrack("a");
    const auto b    = fixture.addTrack("b");
    const auto c    = fixture.addTrack("c");

    REQUIRE(fixture.playlists->setEntries(list, {c, a, b, a}).has_value());
    CHECK(fixture.titles(list) == std::vector<std::string>{"c", "a", "b", "a"});
    CHECK(fixture.playlists->entries(list).value() == std::vector<std::int64_t>{c, a, b, a});

    REQUIRE(fixture.playlists->setEntries(list, {b}).has_value());
    CHECK(fixture.titles(list) == std::vector<std::string>{"b"});
}

TEST_CASE("PlaylistStore: a name is unique")
{
    Fixture fixture;
    REQUIRE(fixture.playlists->create("Set").has_value());
    CHECK_FALSE(fixture.playlists->create("Set").has_value());
}

TEST_CASE("PlaylistStore: tracks removed from the library leave their playlists")
{
    Fixture    fixture;
    const auto list = fixture.playlists->create("Set").value();
    REQUIRE(fixture.playlists->setEntries(list, {fixture.addTrack("gone"), fixture.addTrack("kept")}).has_value());
    // gone.mp3 and kept.mp3 do not exist on disk; only make kept exist
    std::ofstream{fixture.data.path() / "kept.mp3"} << "x";
    const std::atomic<bool> no_stop{false};
    REQUIRE(checkMissingFiles(fixture.tracks, no_stop).empty());

    REQUIRE(fixture.tracks.removeMissing().has_value());

    CHECK(fixture.titles(list) == std::vector<std::string>{"kept"});
    CHECK(fixture.playlists->list().value().front().track_count == 1);
}

TEST_CASE("PlaylistStore: a version 3 database gets the playlist tables")
{
    Fixture fixture;
    fixture.addTrack("a");
    djw::tests::downgradeLibrary(fixture.data.path() / "library.db", 3);

    auto reopened = TrackStore::open(fixture.data.path() / "library.db");
    REQUIRE_MESSAGE(reopened.has_value(), reopened.error());
    auto playlists = PlaylistStore::open(reopened->connection());
    REQUIRE_MESSAGE(playlists.has_value(), playlists.error());

    CHECK(playlists->list().value().empty());
    CHECK(playlists->create("New").has_value());
    CHECK(reopened->trackCount().value() == 1);
}

TEST_CASE("PlaylistStore: entries that cannot be written leave the old ones")
{
    Fixture    fixture;
    const auto a        = fixture.addTrack("a");
    const auto b        = fixture.addTrack("b");
    const auto playlist = fixture.playlists->create("Set").value();
    REQUIRE(fixture.playlists->setEntries(playlist, {a}).has_value());
    const char *part = "";
    int         skip = 0;
    SUBCASE("savepoint")
    {
        part = "SAVEPOINT set_entries";
    }
    SUBCASE("second entry")
    {
        part = "INSERT INTO playlist_entries";
        skip = 1;
    }
    {
        const djw::tests::SqlFault fault{part, skip};
        const auto                 written = fixture.playlists->setEntries(playlist, {b, a, b});
        REQUIRE_FALSE(written.has_value());
        CHECK(written.error().find("injected fault") != std::string::npos);
        CHECK(fault.struck() == 1);
    }
    CHECK(fixture.titles(playlist) == std::vector<std::string>{"a"});
}
