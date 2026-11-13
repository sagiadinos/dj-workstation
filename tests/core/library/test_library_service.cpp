// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "api/library.hpp"
#include "library/library_service.hpp"
#include "library/tag_reader.hpp"

#include "sql_fault.hpp"
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

    CHECK(waitForEdits(*library).write_error == "cannot write tags: track 4711 is no longer in the library");
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

TEST_CASE("Library: moving playlist entries down and up over several places, the ends included")
{
    const djw::tests::TempDir music;
    const djw::tests::TempDir data;
    for (const char *name : {"a", "b", "c", "d", "e"})
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
    const auto      tracks    = library->search({}).value(); // a .. e

    playlists.create("Set");
    revision      = waitForRevision(*library, revision).revision;
    const auto id = playlists.list().value().front().id;
    playlists.add(id, {tracks[0].id, tracks[1].id, tracks[2].id, tracks[3].id, tracks[4].id});

    revision = waitForRevision(*library, revision).revision;
    // One change at a time, each checked, so a move that does nothing cannot hide behind a later one
    const auto after = [&](const auto &change)
    {
        change();
        const auto status = waitForRevision(*library, revision);
        revision          = status.revision;
        CHECK(status.write_error.empty());
        return playlistTitles(playlists, id);
    };
    using Titles = std::vector<std::string>;
    CHECK(after([&] { playlists.moveEntry(id, 1, 3); }) == Titles{"a", "c", "d", "b", "e"}); // down by two
    CHECK(after([&] { playlists.moveEntry(id, 4, 0); }) == Titles{"e", "a", "c", "d", "b"}); // end to start
    CHECK(after([&] { playlists.moveEntry(id, 0, 4); }) == Titles{"a", "c", "d", "b", "e"}); // start to end
    CHECK(after([&] { playlists.moveEntry(id, 3, 1); }) == Titles{"a", "b", "c", "d", "e"}); // up by two
    CHECK(after([&] { playlists.removeEntry(id, 4); }) == Titles{"a", "b", "c", "d"});       // the last one

    // Nothing changes: the same place, beyond the end; renaming to its own name is fine
    playlists.moveEntry(id, 2, 2);
    playlists.moveEntry(id, 4, 0);
    playlists.moveEntry(id, 0, 4);
    playlists.removeEntry(id, 4);
    playlists.rename(id, "Set");
    playlists.create("Marker"); // raises the revision for sure, after all the others
    const auto status = waitForRevision(*library, revision);
    CHECK(status.write_error.empty());
    CHECK(playlistTitles(playlists, id) == Titles{"a", "b", "c", "d"});
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

TEST_CASE("Library: a cue point is stored in the database, the revision goes up, the file stays as it is")
{
    const djw::tests::TempDir music;
    const djw::tests::TempDir data;
    std::filesystem::copy_file(audio_dir / "tagged_vorbis.flac", music.path() / "a.flac");
    auto library = openLibrary(data.path() / "library.db");
    REQUIRE(library->addFolder(music.path()).has_value());
    const api::LibraryStatus scanned = waitForScan(*library);
    const auto               track   = library->search({}).value().front();
    CHECK_FALSE(track.cue_seconds.has_value());

    library->setCuePoint(track.id, 7.25);
    const api::LibraryStatus stored = waitForEdits(*library);

    CHECK(stored.write_error.empty());
    CHECK(stored.revision > scanned.revision);
    const auto found = library->search({}).value();
    REQUIRE(found.size() == 1);
    REQUIRE(found.front().cue_seconds.has_value());
    CHECK(*found.front().cue_seconds == doctest::Approx(7.25));

    // The file is untouched: a rescan reads nothing again, and the cue point is still there
    library->rescan();
    CHECK(waitForScan(*library).files_updated == 0);
    CHECK(library->search({}).value().front().cue_seconds.has_value());

    library->setCuePoint(track.id, std::nullopt);
    static_cast<void>(waitForEdits(*library));
    CHECK_FALSE(library->search({}).value().front().cue_seconds.has_value());
}

namespace
{
    /// Waits until the analysis queue is empty and everything it produced is written, at most 20 seconds
    api::LibraryStatus waitForAnalysis(const api::Library &library)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{20};
        while ((library.status().analysis_pending > 0 || library.status().pending_writes > 0) &&
               std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{5});
        }
        return library.status();
    }
} // namespace

TEST_CASE("Library: analyze stores the waveform, a track is analyzed once, analyzeAll takes the rest")
{
    const djw::tests::TempDir music;
    const djw::tests::TempDir data;
    std::filesystem::copy_file(audio_dir / "sine_440hz_48k_stereo.wav", music.path() / "a.wav");
    std::filesystem::copy_file(audio_dir / "sine_440hz_44k1_mono.wav", music.path() / "b.wav");
    auto library = openLibrary(data.path() / "library.db");
    REQUIRE(library->addFolder(music.path()).has_value());
    waitForScan(*library);
    auto tracks = library->search({.sort_by = api::LibraryColumn::Title}).value();
    REQUIRE(tracks.size() == 2);
    CHECK_FALSE(tracks[0].analyzed);
    CHECK(library->waveform(tracks[0].id).empty());

    const std::uint64_t before = library->status().revision;
    library->analyze(tracks[0].id);
    const api::LibraryStatus done = waitForAnalysis(*library);

    CHECK(done.write_error.empty());
    CHECK(done.revision > before);
    tracks = library->search({.sort_by = api::LibraryColumn::Title}).value();
    CHECK(tracks[0].analyzed);
    CHECK_FALSE(tracks[0].bpm.has_value()); // a sine has no beat
    CHECK_FALSE(library->waveform(tracks[0].id).empty());
    CHECK_FALSE(tracks[1].analyzed);

    // Analyzed once: asking again does nothing
    const std::uint64_t analyzed_revision = library->status().revision;
    library->analyze(tracks[0].id);
    CHECK(library->status().analysis_pending == 0);
    CHECK(library->status().revision == analyzed_revision);

    library->analyzeAll();
    waitForAnalysis(*library);
    tracks = library->search({.sort_by = api::LibraryColumn::Title}).value();
    CHECK(tracks[0].analyzed);
    CHECK(tracks[1].analyzed);
}

TEST_CASE("Library: a file that cannot be decoded is marked analyzed and not tried again")
{
    const djw::tests::TempDir music;
    const djw::tests::TempDir data;
    std::ofstream{music.path() / "Broken - Song.mp3"} << "this is no audio";
    auto library = openLibrary(data.path() / "library.db");
    REQUIRE(library->addFolder(music.path()).has_value());
    waitForScan(*library);

    library->analyzeAll();
    waitForAnalysis(*library);

    const auto tracks = library->search({}).value();
    REQUIRE(tracks.size() == 1);
    CHECK(tracks.front().analyzed);
    CHECK_FALSE(tracks.front().bpm.has_value());
    CHECK(library->waveform(tracks.front().id).empty());
    library->analyzeAll();
    CHECK(library->status().analysis_pending == 0);
}

TEST_CASE("Library: a tag edit keeps the analysis, scanning a track again reads it and analyzes it anew")
{
    const djw::tests::TempDir music;
    const djw::tests::TempDir data;
    std::filesystem::copy_file(audio_dir / "tagged_vorbis.flac", music.path() / "a.flac");
    auto library = openLibrary(data.path() / "library.db");
    REQUIRE(library->addFolder(music.path()).has_value());
    waitForScan(*library);
    const auto id = library->search({}).value().front().id;
    library->analyze(id);
    waitForAnalysis(*library);
    REQUIRE(library->search({}).value().front().analyzed);

    library->updateTags(id, {.title = "New Title"});
    waitForAnalysis(*library);
    auto track = library->search({}).value().front();
    CHECK(track.title == "New Title");
    CHECK(track.analyzed);

    // Change a tag behind djw's back, then scan the track again
    library->rescanTrack(id);
    const api::LibraryStatus status = waitForAnalysis(*library);
    CHECK(status.write_error.empty());
    track = library->search({}).value().front();
    CHECK(track.analyzed); // analyzed again after the rescan
    CHECK_FALSE(library->waveform(id).empty());
}

TEST_CASE("Library: scanning an unknown track again is an error")
{
    const djw::tests::TempDir data;
    auto                      library = openLibrary(data.path() / "library.db");

    library->rescanTrack(4711);
    const api::LibraryStatus status = waitForAnalysis(*library);

    CHECK(status.write_error == "cannot scan again: track 4711 is no longer in the library");
}

namespace
{
    /// A service over these files in music, their titles from the file names, tag writing always fine
    std::unique_ptr<library::LibraryService> scannedService(const djw::tests::TempDir          &music,
                                                            const djw::tests::TempDir          &data,
                                                            std::initializer_list<const char *> names)
    {
        for (const char *name : names)
        {
            std::ofstream{music.path() / name} << "x";
        }
        auto service = openService(
            data.path() / "library.db",
            [](const std::filesystem::path &file) { return library::TrackTags{.title = file.stem().string()}; },
            [](const std::filesystem::path &, const api::LibraryTagChange &) -> std::expected<void, std::string>
            { return {}; });
        REQUIRE(service->addFolder(music.path()).has_value());
        REQUIRE(waitForScan(*service).tracks_total == names.size());
        return service;
    }

    /// The status once the write queued last is done, and while the fault is still there
    api::LibraryStatus afterWrite(const api::Library &library, std::uint64_t revision)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
        while ((library.status().pending_writes > 0 ||
                (library.status().write_error.empty() && library.status().revision <= revision)) &&
               std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{2});
        }
        return library.status();
    }
} // namespace

TEST_CASE("Library: a library whose reading or writing connection cannot be opened is an error")
{
    const djw::tests::TempDir data;
    const char               *part = "";
    int                       skip = 0;
    SUBCASE("reading tracks")
    {
        part = "BEGIN EXCLUSIVE";
    }
    SUBCASE("writing tracks")
    {
        part = "BEGIN EXCLUSIVE";
        skip = 1;
    }
    SUBCASE("reading playlists")
    {
        part = "SELECT id FROM playlists WHERE name";
    }
    SUBCASE("writing playlists")
    {
        part = "SELECT id FROM playlists WHERE name";
        skip = 1;
    }
    const djw::tests::SqlFault fault{part, skip};
    const auto                 opened = library::LibraryService::open(
        data.path() / "library.db", [](const std::filesystem::path &) { return library::TrackTags{}; },
        [](const std::filesystem::path &, const api::LibraryTagChange &) -> std::expected<void, std::string>
        { return {}; });
    REQUIRE_FALSE(opened.has_value());
    CHECK(opened.error().find("injected fault") != std::string::npos);
    CHECK(fault.struck() == 1);
}

TEST_CASE("Library: the counts are there right after opening, the missing tracks included")
{
    const djw::tests::TempDir music;
    const djw::tests::TempDir data;
    {
        auto service = scannedService(music, data, {"a.mp3", "b.mp3"});
        std::filesystem::remove(music.path() / "b.mp3");
        service->rescan();
        REQUIRE(waitForScan(*service).missing_total == 1);
    }
    const auto status = openLibrary(data.path() / "library.db")->status();
    CHECK(status.tracks_total == 2);
    CHECK(status.missing_total == 1);
}

TEST_CASE("Library: a file is not a folder to add, and a folder that cannot be stored is an error")
{
    const djw::tests::TempDir data;
    std::ofstream{data.path() / "file.mp3"} << "x";
    auto library = openLibrary(data.path() / "library.db");
    CHECK_FALSE(library->addFolder(data.path() / "file.mp3").has_value());

    {
        const djw::tests::SqlFault fault{"INSERT OR IGNORE INTO folders"};
        const auto                 added = library->addFolder(data.path());
        REQUIRE_FALSE(added.has_value());
        CHECK(added.error().find("injected fault") != std::string::npos);
        CHECK(fault.struck() == 1);
    }
    CHECK(library->folders().empty());
    CHECK_FALSE(library->status().scanning);
}

TEST_CASE("Library: a track whose state cannot be read is not analyzed")
{
    const djw::tests::TempDir music;
    const djw::tests::TempDir data;
    auto                      service = scannedService(music, data, {"a.mp3"});
    const auto                id      = service->search({}).value().front().id;
    const char               *part    = "";
    SUBCASE("analyzed")
    {
        part = "SELECT analyzed FROM tracks";
    }
    SUBCASE("path")
    {
        part = "SELECT path FROM tracks WHERE id";
    }
    SUBCASE("all")
    {
        part = "WHERE analyzed = 0";
    }
    const djw::tests::SqlFault fault{part};
    if (std::string_view{part} == "WHERE analyzed = 0")
    {
        service->analyzeAll();
    }
    else
    {
        service->analyze(id);
    }
    CHECK(fault.struck() == 1);
    CHECK(service->status().analysis_pending == 0);
}

TEST_CASE("Library: an unknown track is not analyzed")
{
    const djw::tests::TempDir data;
    auto                      library = openLibrary(data.path() / "library.db");
    library->analyze(4711);
    CHECK(library->status().analysis_pending == 0);
}

TEST_CASE("Library: a scan that cannot read the folders or check the files reports it")
{
    const djw::tests::TempDir music;
    const djw::tests::TempDir data;
    auto                      service = scannedService(music, data, {"a.mp3"});
    const auto                before  = service->status().revision;
    const char               *part    = "";
    SUBCASE("folders")
    {
        part = "SELECT path FROM folders";
    }
    SUBCASE("missing files")
    {
        part = "SELECT id, path, missing FROM tracks";
    }
    api::LibraryStatus status;
    {
        const djw::tests::SqlFault fault{part};
        service->rescan();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
        while (service->status().revision == before && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{2});
        }
        status = service->status();
        CHECK(fault.struck() > 0);
    }
    CHECK_FALSE(status.scanning);
    CHECK(status.scan_error.find("injected fault") != std::string::npos);
    CHECK(status.tracks_total == 1);
}

TEST_CASE("Library: a failed write reports its error and keeps the revision, the next good write clears it")
{
    const djw::tests::TempDir music;
    const djw::tests::TempDir data;
    auto                      service = scannedService(music, data, {"a.mp3"});
    const auto                id      = service->search({}).value().front().id;
    const auto                before  = service->status().revision;
    {
        const djw::tests::SqlFault fault{"UPDATE tracks SET cue"};
        service->setCuePoint(id, 1.5);
        const auto failed = afterWrite(*service, before);
        CHECK(fault.struck() == 1);
        CHECK(failed.write_error.find("injected fault") != std::string::npos);
        CHECK(failed.revision == before);
    }
    CHECK_FALSE(service->search({}).value().front().cue_seconds.has_value());

    service->setCuePoint(id, 1.5);
    const auto stored = afterWrite(*service, before);
    CHECK(stored.write_error.empty());
    CHECK(stored.revision > before);
}

TEST_CASE("Library: playlist changes the database refuses report their error")
{
    const djw::tests::TempDir music;
    const djw::tests::TempDir data;
    auto                      service   = scannedService(music, data, {"a.mp3", "b.mp3"});
    api::Playlists           &playlists = service->playlists();
    const auto                tracks    = service->search({}).value();
    playlists.create("Set");
    auto revision = afterWrite(*service, service->status().revision).revision;
    REQUIRE(playlists.list().value().size() == 1);
    const auto id = playlists.list().value().front().id;

    const char           *part = "";
    std::function<void()> change;
    SUBCASE("name check of a new playlist")
    {
        part   = "SELECT id FROM playlists WHERE name";
        change = [&] { playlists.create("Other"); };
    }
    SUBCASE("new playlist")
    {
        part   = "INSERT INTO playlists";
        change = [&] { playlists.create("Other"); };
    }
    SUBCASE("name check of a rename")
    {
        part   = "SELECT id FROM playlists WHERE name";
        change = [&] { playlists.rename(id, "Other"); };
    }
    SUBCASE("entries")
    {
        part   = "SELECT track_id FROM playlist_entries";
        change = [&] { playlists.add(id, {tracks[0].id}); };
    }
    {
        const djw::tests::SqlFault fault{part};
        change();
        const auto status = afterWrite(*service, revision);
        CHECK(fault.struck() == 1);
        CHECK(status.write_error.find("injected fault") != std::string::npos);
        CHECK(status.revision == revision);
    }
    const auto lists = playlists.list().value();
    REQUIRE(lists.size() == 1);
    CHECK(lists.front().name == "Set");
    CHECK(playlists.tracks(id).value().empty());
}

TEST_CASE("Library: a rename to the name of another playlist is refused, to its own name it is fine")
{
    const djw::tests::TempDir data;
    auto                      library   = openLibrary(data.path() / "library.db");
    api::Playlists           &playlists = library->playlists();
    playlists.create("A");
    auto status = afterWrite(*library, 0);
    playlists.create("B");
    status = afterWrite(*library, status.revision);
    REQUIRE(playlists.list().value().size() == 2);
    const auto lists = playlists.list().value();
    const auto a     = lists[0].name == "A" ? lists[0].id : lists[1].id;

    playlists.rename(a, "B");
    const auto refused = afterWrite(*library, status.revision);
    CHECK(refused.write_error.find("exists") != std::string::npos);

    playlists.rename(a, "A");
    const auto kept = afterWrite(*library, refused.revision);
    CHECK(kept.write_error.empty());
    CHECK(kept.revision > refused.revision);
}

TEST_CASE("Library: an M3U file that cannot be read is an error, the last import stays")
{
    const djw::tests::TempDir data;
    auto                      library = openLibrary(data.path() / "library.db");
    library->playlists().importM3u(data.path() / "nowhere.m3u");
    const auto status = afterWrite(*library, 0);
    CHECK_FALSE(status.write_error.empty());
    CHECK_FALSE(status.last_import.has_value());
    CHECK(library->playlists().list().value().empty());
}

TEST_CASE("Library: a track whose file is gone cannot be scanned again and is not analyzed")
{
    const djw::tests::TempDir music;
    const djw::tests::TempDir data;
    auto                      service = scannedService(music, data, {"a.mp3"});
    const auto                id      = service->search({}).value().front().id;
    std::filesystem::remove(music.path() / "a.mp3");

    service->rescanTrack(id);
    const auto status = afterWrite(*service, service->status().revision);

    CHECK(status.write_error.starts_with("cannot read "));
    CHECK(waitForAnalysis(*service).analysis_pending == 0);
    CHECK_FALSE(service->search({}).value().front().analyzed);
}

TEST_CASE("Library: every scan counts its files anew")
{
    const djw::tests::TempDir music;
    const djw::tests::TempDir data;
    auto                      service = scannedService(music, data, {"a.mp3", "b.mp3"});
    CHECK(service->status().files_found == 2);
    service->rescan();
    const auto status = waitForScan(*service);
    CHECK(status.files_found == 2);
    CHECK(status.files_updated == 0);
}

TEST_CASE("Library: the pending writes and analyses count what is queued and running, never more")
{
    const djw::tests::TempDir music;
    const djw::tests::TempDir data;
    std::filesystem::copy_file(audio_dir / "sine_440hz_48k_stereo.wav", music.path() / "a.wav");
    auto library = openLibrary(data.path() / "library.db");
    REQUIRE(library->addFolder(music.path()).has_value());
    waitForScan(*library);
    const auto id = library->search({}).value().front().id;

    library->setCuePoint(id, 1.0);
    library->analyze(id);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{20};
    while (std::chrono::steady_clock::now() < deadline)
    {
        const auto status = library->status();
        REQUIRE(status.pending_writes <= 2); // the cue point and the analysis result
        REQUIRE(status.analysis_pending <= 1);
        if (status.pending_writes == 0 && status.analysis_pending == 0 && library->search({}).value().front().analyzed)
        {
            break;
        }
        std::this_thread::sleep_for(std::chrono::microseconds{200});
    }
    CHECK(library->search({}).value().front().analyzed);
}
