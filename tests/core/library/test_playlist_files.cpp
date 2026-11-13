// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library/m3u.hpp"
#include "library/playlist_files.hpp"
#include "library/tag_reader.hpp"

#include "sql_fault.hpp"
#include "temp_dir.hpp"

#include <doctest/doctest.h>

#include <fstream>
#include <iterator>

using namespace djw::core::library;

namespace
{
    const std::filesystem::path audio_dir{DJW_TEST_AUDIO_DIR};

    struct Fixture
    {
            Fixture() : tracks{TrackStore::open(data.path() / "library.db").value()}
            {
                playlists.emplace(PlaylistStore::open(tracks.connection()).value());
                std::filesystem::copy_file(audio_dir / "tagged_id3v2.mp3", music.path() / "known.mp3");
                std::filesystem::copy_file(audio_dir / "tagged_vorbis.flac", music.path() / "new.flac");
                // known.mp3 is in the library, new.flac is not
                REQUIRE(tracks.store(music.path() / "known.mp3", {.modified = 1, .size = 1}, {.title = "Known"})
                            .has_value());
            }

            std::filesystem::path writeList(const std::string &name, const std::string &content) const
            {
                const auto file = music.path() / name;
                std::ofstream{file, std::ios::binary} << content;
                return file;
            }

            djw::tests::TempDir          music;
            djw::tests::TempDir          data;
            TrackStore                   tracks;
            std::optional<PlaylistStore> playlists;
    };
} // namespace

TEST_CASE("importM3uFile: library tracks are used, new audio files are added with tags, missing ones counted")
{
    Fixture    fixture;
    const auto list = fixture.writeList("Friday Set.m3u8", "#EXTM3U\nknown.mp3\nnew.flac\nmissing.mp3\nknown.mp3\n");

    const auto imported = importM3uFile(list, fixture.tracks, *fixture.playlists, readTags);

    REQUIRE_MESSAGE(imported.has_value(), imported.error());
    CHECK(imported->playlist == "Friday Set");
    CHECK(imported->tracks == 3);
    CHECK(imported->not_found == 1);
    const auto id     = fixture.playlists->idOf("Friday Set").value().value();
    const auto titles = fixture.playlists->tracks(id).value();
    REQUIRE(titles.size() == 3);
    CHECK(titles[0].title == "Known");
    CHECK(titles[1].title == "Ünïcode Title"); // read from the file
    CHECK(titles[2].title == "Known");
    CHECK(fixture.tracks.trackCount().value() == 2);
}

TEST_CASE("importM3uFile: a taken name gets a number")
{
    Fixture    fixture;
    const auto list = fixture.writeList("Set.m3u", "known.mp3\n");
    REQUIRE(importM3uFile(list, fixture.tracks, *fixture.playlists, readTags).has_value());
    REQUIRE(importM3uFile(list, fixture.tracks, *fixture.playlists, readTags).has_value());

    const auto imported = importM3uFile(list, fixture.tracks, *fixture.playlists, readTags);

    REQUIRE(imported.has_value());
    CHECK(imported->playlist == "Set (3)");
    CHECK(fixture.playlists->list().value().size() == 3);
}

TEST_CASE("importM3uFile: a file that cannot be read is an error and changes nothing")
{
    Fixture    fixture;
    const auto imported =
        importM3uFile(fixture.music.path() / "nope.m3u", fixture.tracks, *fixture.playlists, readTags);

    REQUIRE_FALSE(imported.has_value());
    CHECK(imported.error().find("nope.m3u") != std::string::npos);
    CHECK(fixture.playlists->list().value().empty());
}

TEST_CASE("exportM3uFile: writes the playlist, which imports again to the same tracks")
{
    Fixture    fixture;
    const auto list = fixture.writeList("Set.m3u8", "known.mp3\nnew.flac\n");
    REQUIRE(importM3uFile(list, fixture.tracks, *fixture.playlists, readTags).has_value());
    const auto id     = fixture.playlists->idOf("Set").value().value();
    const auto output = fixture.data.path() / "Exported.m3u8";

    REQUIRE(exportM3uFile(id, output, *fixture.playlists).has_value());

    std::ifstream     in{output, std::ios::binary};
    const std::string text{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    CHECK(text.starts_with("#EXTM3U\n#EXTINF:0,Known\n"));
    CHECK(text.find((fixture.music.path() / "new.flac").string()) != std::string::npos);

    const auto again = importM3uFile(output, fixture.tracks, *fixture.playlists, readTags);
    REQUIRE(again.has_value());
    CHECK(again->tracks == 2);
    CHECK(again->not_found == 0);
}

TEST_CASE("exportM3uFile: a file that cannot be written is an error naming it")
{
    Fixture    fixture;
    const auto id      = fixture.playlists->create("Empty").value();
    const auto written = exportM3uFile(id, fixture.data.path() / "no" / "such" / "folder.m3u", *fixture.playlists);
    REQUIRE_FALSE(written.has_value());
    CHECK(written.error().find("folder.m3u") != std::string::npos);
}

TEST_CASE("importM3uFile: a database error on the way is returned and changes nothing")
{
    Fixture     fixture;
    const auto  list = fixture.writeList("Set.m3u", "known.mp3\nnew.flac\n");
    const char *part = "";
    SUBCASE("transaction")
    {
        part = "BEGIN";
    }
    SUBCASE("looking up a track")
    {
        part = "SELECT id FROM tracks WHERE path";
    }
    SUBCASE("adding a track")
    {
        part = "INSERT INTO tracks";
    }
    SUBCASE("looking up the name")
    {
        part = "SELECT id FROM playlists WHERE name";
    }
    SUBCASE("creating the playlist")
    {
        part = "INSERT INTO playlists (name)";
    }
    SUBCASE("writing the entries")
    {
        part = "SAVEPOINT set_entries";
    }
    {
        const djw::tests::SqlFault fault{part};
        const auto                 imported = importM3uFile(list, fixture.tracks, *fixture.playlists, readTags);
        REQUIRE_FALSE(imported.has_value());
        CHECK(imported.error().find("injected fault") != std::string::npos);
        CHECK(fault.struck() > 0);
    }
    CHECK(fixture.playlists->list().value().empty());
    CHECK(fixture.tracks.trackCount().value() == 1);
}

TEST_CASE("importM3uFile: an import that cannot be committed is an error")
{
    Fixture                    fixture;
    const auto                 list = fixture.writeList("Set.m3u", "known.mp3\n");
    const djw::tests::SqlFault fault{"COMMIT"};
    const auto                 imported = importM3uFile(list, fixture.tracks, *fixture.playlists, readTags);
    REQUIRE_FALSE(imported.has_value());
    CHECK(imported.error().find("injected fault") != std::string::npos);
}

TEST_CASE("importM3uFile: an error that cannot be rolled back reports both errors")
{
    Fixture                    fixture;
    const djw::tests::SqlFault fault{"ROLLBACK"};
    const auto                 imported =
        importM3uFile(fixture.music.path() / "missing.m3u", fixture.tracks, *fixture.playlists, readTags);
    REQUIRE_FALSE(imported.has_value());
    CHECK(imported.error().find("cannot read") != std::string::npos);
    CHECK(imported.error().find("injected fault") != std::string::npos);
}

TEST_CASE("exportM3uFile: the tracks that cannot be read are an error, an existing file is replaced as a whole")
{
    Fixture    fixture;
    const auto playlist = fixture.playlists->create("Empty").value();
    const auto file     = fixture.music.path() / "out.m3u";
    {
        const djw::tests::SqlFault fault{"FROM playlist_entries e JOIN tracks"};
        const auto                 exported = exportM3uFile(playlist, file, *fixture.playlists);
        REQUIRE_FALSE(exported.has_value());
        CHECK(exported.error().find("injected fault") != std::string::npos);
        CHECK_FALSE(std::filesystem::exists(file));
    }
    std::ofstream{file} << std::string(1'000, 'x');
    REQUIRE(exportM3uFile(playlist, file, *fixture.playlists).has_value());
    std::ifstream     in{file, std::ios::binary};
    const std::string text{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    CHECK(text == formatM3u({}));
}
