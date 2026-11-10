// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "api/library.hpp"
#include "library/library_service.hpp"
#include "library/tag_reader.hpp"

#include "temp_dir.hpp"

#include <doctest/doctest.h>

#include <chrono>
#include <fstream>
#include <atomic>
#include <thread>

using namespace djw::core;

namespace
{
    const std::filesystem::path audio_dir{DJW_TEST_AUDIO_DIR};

    /// Waits until no scan is running any more, at most five seconds
    api::LibraryStatus waitForScan(const api::Library &library)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
        while (library.status().scanning && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{5});
        }
        return library.status();
    }

    std::unique_ptr<api::Library> openLibrary(const std::filesystem::path &database_file)
    {
        auto library = api::openLibrary({.database_file = database_file});
        REQUIRE_MESSAGE(library.has_value(), library.error());
        return std::move(*library);
    }
} // namespace

TEST_CASE("Library: a folder added is scanned with real tags and can be searched")
{
    const djw::tests::TempDir music;
    const djw::tests::TempDir data;
    std::filesystem::copy_file(audio_dir / "tagged_id3v2.mp3", music.path() / "tagged_id3v2.mp3");
    std::filesystem::create_directories(music.path() / "sub");
    std::filesystem::copy_file(audio_dir / "tagged_vorbis.flac", music.path() / "sub" / "tagged_vorbis.flac");
    std::filesystem::copy_file(audio_dir / "sine_440hz_48k_stereo.wav", music.path() / "Some One - Some Song.wav");

    auto library = openLibrary(data.path() / "new" / "library.db"); // the folder is created
    REQUIRE(library->addFolder(music.path()).has_value());
    const api::LibraryStatus status = waitForScan(*library);

    CHECK_FALSE(status.scanning);
    CHECK(status.scan_error.empty());
    CHECK(status.files_found == 3);
    CHECK(status.tracks_total == 3);
    CHECK(library->folders() == std::vector<std::filesystem::path>{std::filesystem::canonical(music.path())});

    const auto beyonce = library->search({.text = "beyonce"});
    REQUIRE(beyonce.has_value());
    REQUIRE(beyonce->size() == 1);
    CHECK(beyonce->front().title == "Ünïcode Title");
    CHECK(beyonce->front().path == std::filesystem::canonical(music.path()) / "sub" / "tagged_vorbis.flac");

    const auto by_name = library->search({.text = "some song"});
    REQUIRE(by_name.has_value());
    REQUIRE(by_name->size() == 1);
    CHECK(by_name->front().artist == "Some One");
}

TEST_CASE("Library: tracks and folders are still there after reopening, a rescan finds new files only")
{
    const djw::tests::TempDir music;
    const djw::tests::TempDir data;
    const auto                database = data.path() / "library.db";
    std::filesystem::copy_file(audio_dir / "tagged_id3v2.mp3", music.path() / "a.mp3");
    {
        auto library = openLibrary(database);
        REQUIRE(library->addFolder(music.path()).has_value());
        waitForScan(*library);
    }

    auto library = openLibrary(database);
    CHECK(library->status().tracks_total == 1);
    std::filesystem::copy_file(audio_dir / "tagged_vorbis.flac", music.path() / "b.flac");
    library->rescan();
    const api::LibraryStatus status = waitForScan(*library);

    CHECK(status.files_found == 2);
    CHECK(status.files_updated == 1);
    CHECK(status.tracks_total == 2);
}

TEST_CASE("Library: adding something that is not a folder is an error")
{
    const djw::tests::TempDir data;
    auto                      library = openLibrary(data.path() / "library.db");

    const auto added = library->addFolder(data.path() / "nowhere");

    REQUIRE_FALSE(added.has_value());
    CHECK(added.error().find("nowhere") != std::string::npos);
    CHECK(library->folders().empty());
}

TEST_CASE("Library: search works while a scan writes, and closing stops a running scan")
{
    const djw::tests::TempDir music;
    const djw::tests::TempDir data;
    for (int i = 0; i < 300; ++i)
    {
        std::ofstream{music.path() / ("Artist - Track " + std::to_string(i) + ".mp3")} << "x";
    }
    auto library = openLibrary(data.path() / "library.db");
    REQUIRE(library->addFolder(music.path()).has_value());

    for (int i = 0; i < 20; ++i)
    {
        const auto found = library->search({.text = "artist"});
        REQUIRE_MESSAGE(found.has_value(), found.error());
        CHECK(found->size() <= 300);
    }
    library.reset(); // must not hang or crash, wherever the scan is
}

TEST_CASE("Library: a database the library cannot create is an error")
{
    const djw::tests::TempDir data;
    std::ofstream{data.path() / "file"} << "x";

    const auto library = api::openLibrary({.database_file = data.path() / "file" / "library.db"});

    CHECK_FALSE(library.has_value());
}

TEST_CASE("Library: opening, scanning and closing many times leaves nothing behind")
{
    const djw::tests::TempDir music;
    const djw::tests::TempDir data;
    std::filesystem::copy_file(audio_dir / "tagged_id3v2.mp3", music.path() / "a.mp3");
    for (int round = 0; round < 20; ++round)
    {
        auto library = openLibrary(data.path() / "library.db");
        REQUIRE(library->addFolder(music.path()).has_value());
        CHECK(waitForScan(*library).tracks_total == 1);
        CHECK(library->search({.text = "tagged"}).value().size() == 1);
    }
}

namespace
{
    /// Waits until no tag edit is pending, at most five seconds
    api::LibraryStatus waitForEdits(const api::Library &library)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
        while (library.status().pending_writes > 0 && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{2});
        }
        return library.status();
    }

    std::unique_ptr<library::LibraryService> openService(const std::filesystem::path &database, library::TagReader read,
                                                         library::TagWriter write)
    {
        auto service = library::LibraryService::open(database, std::move(read), std::move(write));
        REQUIRE_MESSAGE(service.has_value(), service.error());
        return std::move(*service);
    }
} // namespace

TEST_CASE("Library: a tag edit is written into the file and the database, and the revision goes up")
{
    const djw::tests::TempDir music;
    const djw::tests::TempDir data;
    std::filesystem::copy_file(audio_dir / "tagged_vorbis.flac", music.path() / "a.flac");
    auto library = openLibrary(data.path() / "library.db");
    REQUIRE(library->addFolder(music.path()).has_value());
    const api::LibraryStatus scanned = waitForScan(*library);
    const auto               track   = library->search({}).value().front();

    library->updateTags(track.id, {.title = "Edited", .year = 2001});
    const api::LibraryStatus edited = waitForEdits(*library);

    CHECK(edited.write_error.empty());
    CHECK(edited.revision > scanned.revision);
    const auto found = library->search({.title = "edited"}).value();
    REQUIRE(found.size() == 1);
    CHECK(found.front().year == 2001);
    CHECK(found.front().artist == "Beyoncé");
    CHECK(library::readTags(found.front().path).title == "Edited");

    // The file changed, the database knows its new stamp: a rescan reads nothing again
    library->rescan();
    CHECK(waitForScan(*library).files_updated == 0);
}

TEST_CASE("Library: a failing tag edit reports its error and changes nothing")
{
    const djw::tests::TempDir music;
    const djw::tests::TempDir data;
    std::ofstream{music.path() / "a.mp3"} << "x";
    auto library = openService(
        data.path() / "library.db", [](const std::filesystem::path &) { return library::TrackTags{.title = "Old"}; },
        [](const std::filesystem::path &, const api::LibraryTagChange &) -> std::expected<void, std::string>
        { return std::unexpected{std::string{"disk full"}}; });
    REQUIRE(library->addFolder(music.path()).has_value());
    const auto revision = waitForScan(*library).revision;

    library->updateTags(library->search({}).value().front().id, {.title = "New"});
    const api::LibraryStatus status = waitForEdits(*library);

    CHECK(status.write_error == "disk full");
    CHECK(status.revision == revision);
    CHECK(library->search({}).value().front().title == "Old");
}

TEST_CASE("Library: a tag edit of an unknown track is an error")
{
    const djw::tests::TempDir data;
    auto                      library = openLibrary(data.path() / "library.db");

    library->updateTags(4711, {.title = "x"});

    CHECK(waitForEdits(*library).write_error.find("4711") != std::string::npos);
}

TEST_CASE("Library: tag edits are written during a scan, not after it")
{
    const djw::tests::TempDir music;
    const djw::tests::TempDir data;
    std::ofstream{music.path() / "first.mp3"} << "x";
    {
        // A library that already knows first.mp3
        auto library = openLibrary(data.path() / "library.db");
        REQUIRE(library->addFolder(music.path()).has_value());
        waitForScan(*library);
    }
    for (int i = 0; i < 200; ++i)
    {
        std::ofstream{music.path() / ("slow" + std::to_string(i) + ".mp3")} << "x";
    }
    std::atomic<int> written{0};
    auto             library = openService(
        data.path() / "library.db",
        [](const std::filesystem::path &file)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{5}); // 200 new files: a scan of at least 1 s
            return library::TrackTags{.title = file.stem().string()};
        },
        [&written](const std::filesystem::path &, const api::LibraryTagChange &) -> std::expected<void, std::string>
        {
            ++written;
            return {};
        });
    const auto first = library->search({.title = "first"}).value().front();

    library->rescan();
    while (library->status().files_found < 5)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    library->updateTags(first.id, {.genre = "x"});
    const api::LibraryStatus status = waitForEdits(*library);

    CHECK(written == 1);
    CHECK(status.scanning); // the scan is still running
    CHECK(status.files_found < 200);
    CHECK(waitForScan(*library).tracks_total == 201);
}

TEST_CASE("Library: a scan marks deleted files as missing, removeMissing takes them out")
{
    const djw::tests::TempDir music;
    const djw::tests::TempDir data;
    std::filesystem::copy_file(audio_dir / "tagged_id3v2.mp3", music.path() / "kept.mp3");
    std::filesystem::copy_file(audio_dir / "tagged_vorbis.flac", music.path() / "deleted.flac");
    auto library = openLibrary(data.path() / "library.db");
    REQUIRE(library->addFolder(music.path()).has_value());
    REQUIRE(waitForScan(*library).missing_total == 0);

    std::filesystem::remove(music.path() / "deleted.flac");
    library->rescan();
    const api::LibraryStatus scanned = waitForScan(*library);

    CHECK(scanned.missing_total == 1);
    CHECK(scanned.tracks_total == 2);
    const auto missing = library->search({.only_missing = true}).value();
    REQUIRE(missing.size() == 1);
    CHECK(missing.front().title == "Ünïcode Title");

    library->removeMissing();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (library->status().revision == scanned.revision && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{2});
    }
    const api::LibraryStatus removed = library->status();

    CHECK(removed.missing_total == 0);
    CHECK(removed.tracks_total == 1);
    CHECK(library->search({}).value().front().title == "Tagged Title");
}

namespace
{
    /// Waits until the library's revision passes before, at most five seconds
    api::LibraryStatus waitForRevision(const api::Library &library, std::uint64_t before)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
        while ((library.status().revision <= before || library.status().pending_writes > 0) &&
               std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{2});
        }
        return library.status();
    }

    std::vector<std::string> playlistTitles(api::Playlists &playlists, std::int64_t id)
    {
        std::vector<std::string> titles;
        const auto               found = playlists.tracks(id).value();
        for (const auto &track : found)
        {
            titles.push_back(track.title);
        }
        return titles;
    }
} // namespace

TEST_CASE("Library: playlist changes run on the library thread and raise the revision")
{
    const djw::tests::TempDir music;
    const djw::tests::TempDir data;
    for (const char *name : {"a", "b", "c"})
    {
        std::ofstream{music.path() / (std::string{name} + ".mp3")} << "x";
    }
    auto library = openService(
        data.path() / "library.db",
        [](const std::filesystem::path &file) { return library::TrackTags{.title = file.stem().string()}; },
        [](const std::filesystem::path &, const api::LibraryTagChange &) -> std::expected<void, std::string>
        { return {}; });
    REQUIRE(library->addFolder(music.path()).has_value());
    auto            revision  = waitForScan(*library).revision;
    api::Playlists &playlists = library->playlists();
    const auto      tracks    = library->search({}).value(); // a, b, c

    playlists.create("Set");
    revision      = waitForRevision(*library, revision).revision;
    const auto id = playlists.list().value().front().id;

    playlists.add(id, {tracks[0].id, tracks[1].id, tracks[2].id});
    playlists.moveEntry(id, 2, 0); // c a b
    playlists.moveEntry(id, 0, 1); // a c b
    playlists.removeEntry(id, 2);  // a c
    playlists.removeEntry(id, 99); // nothing
    playlists.rename(id, "Friday");
    const auto status = waitForRevision(*library, revision);

    CHECK(status.write_error.empty());
    CHECK(playlistTitles(playlists, id) == std::vector<std::string>{"a", "c"});
    CHECK(playlists.list().value().front().name == "Friday");

    playlists.create("Friday");
    CHECK(waitForRevision(*library, status.revision).write_error.find("exists") != std::string::npos);

    playlists.remove(id);
    waitForRevision(*library, library->status().revision);
    CHECK(playlists.list().value().empty());
}

TEST_CASE("Library: writes still queued are done when the library closes")
{
    const djw::tests::TempDir data;
    {
        auto library = openLibrary(data.path() / "library.db");
        library->playlists().create("Saved before exit");
    }
    auto library = openLibrary(data.path() / "library.db");
    REQUIRE(library->playlists().list().value().size() == 1);
    CHECK(library->playlists().list().value().front().name == "Saved before exit");
}

TEST_CASE("Library: an M3U import reports its outcome in the status")
{
    const djw::tests::TempDir music;
    const djw::tests::TempDir data;
    std::filesystem::copy_file(audio_dir / "tagged_id3v2.mp3", music.path() / "a.mp3");
    std::ofstream{music.path() / "Set.m3u"} << "a.mp3\nmissing.mp3\n";
    auto library = openLibrary(data.path() / "library.db");

    library->playlists().importM3u(music.path() / "Set.m3u");
    const auto status = waitForRevision(*library, 0);

    REQUIRE(status.last_import.has_value());
    CHECK(status.last_import->playlist == "Set");
    CHECK(status.last_import->tracks == 1);
    CHECK(status.last_import->not_found == 1);
    CHECK(status.tracks_total == 1);
}
