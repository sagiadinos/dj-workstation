// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library/folder_scanner.hpp"

#include "temp_dir.hpp"

#include <doctest/doctest.h>

#include <chrono>
#include <fstream>
#include <map>

using namespace djw::core::library;

namespace
{
    /// Tags made from the file name, counting how often each file was read
    struct CountingTagReader
    {
            TrackTags operator()(const std::filesystem::path &file)
            {
                ++(*reads)[file.filename().string()];
                return {.title = file.stem().string()};
            }

            std::shared_ptr<std::map<std::string, int>> reads = std::make_shared<std::map<std::string, int>>();
    };

    struct Fixture
    {
            Fixture() : store{openStore()} {}

            TrackStore openStore() const
            {
                auto opened = TrackStore::open(database.path() / "library.db");
                REQUIRE(opened.has_value());
                return std::move(*opened);
            }

            void write(const std::filesystem::path &relative, std::string_view content = "audio") const
            {
                const auto file = music.path() / relative;
                std::filesystem::create_directories(file.parent_path());
                std::ofstream{file, std::ios::binary} << content;
            }

            std::string scan(const std::atomic<bool> &stop = no_stop)
            {
                counters.files_found   = 0;
                counters.files_updated = 0;
                return scanFolders({music.path()}, store, reader, counters, stop);
            }

            std::vector<std::string> titles()
            {
                std::vector<std::string> result;
                const auto               found = store.search("", 1'000).value();
                for (const auto &track : found)
                {
                    result.push_back(track.title);
                }
                return result;
            }

            static inline const std::atomic<bool> no_stop{false};

            djw::tests::TempDir music;
            djw::tests::TempDir database;
            TrackStore          store;
            CountingTagReader   reader;
            ScanCounters        counters;
    };
} // namespace

TEST_CASE("isAudioFile: the four formats, in any case")
{
    CHECK(isAudioFile("a.mp3"));
    CHECK(isAudioFile("a.OGG"));
    CHECK(isAudioFile("a.Flac"));
    CHECK(isAudioFile("/x/y.wav"));
    CHECK_FALSE(isAudioFile("a.txt"));
    CHECK_FALSE(isAudioFile("mp3"));
    CHECK_FALSE(isAudioFile("a.mp3.part"));
}

TEST_CASE("scanFolders: finds audio files in subfolders and skips everything else")
{
    Fixture fixture;
    fixture.write("one.mp3");
    fixture.write("house/two.FLAC");
    fixture.write("house/deep/three.wav");
    fixture.write("cover.jpg");
    fixture.write("notes.txt");

    CHECK(fixture.scan().empty());

    CHECK(fixture.titles() == std::vector<std::string>{"one", "three", "two"});
    CHECK(fixture.counters.files_found == 3);
    CHECK(fixture.counters.files_updated == 3);
}

TEST_CASE("scanFolders: a rescan reads only new and changed files")
{
    Fixture fixture;
    fixture.write("same.mp3");
    fixture.write("grows.mp3", "short");
    fixture.write("touched.mp3");
    REQUIRE(fixture.scan().empty());

    fixture.write("grows.mp3", "longer content");
    const auto touched = fixture.music.path() / "touched.mp3";
    std::filesystem::last_write_time(touched, std::filesystem::last_write_time(touched) + std::chrono::seconds{10});
    fixture.write("new.mp3");
    REQUIRE(fixture.scan().empty());

    CHECK(fixture.counters.files_found == 4);
    CHECK(fixture.counters.files_updated == 3);
    const auto &reads = *fixture.reader.reads;
    CHECK(reads.at("same.mp3") == 1);
    CHECK(reads.at("grows.mp3") == 2);
    CHECK(reads.at("touched.mp3") == 2);
    CHECK(reads.at("new.mp3") == 1);
}

TEST_CASE("scanFolders: more files than one batch are all stored")
{
    Fixture fixture;
    for (int i = 0; i < 250; ++i)
    {
        fixture.write("track" + std::to_string(i) + ".mp3");
    }

    CHECK(fixture.scan().empty());

    CHECK(fixture.store.trackCount().value() == 250);
}

TEST_CASE("scanFolders: a missing folder is reported, the other folders are scanned")
{
    Fixture fixture;
    fixture.write("found.mp3");
    const auto missing = fixture.music.path() / "missing";

    const std::string error =
        scanFolders({missing, fixture.music.path()}, fixture.store, fixture.reader, fixture.counters, Fixture::no_stop);

    CHECK(error.find("missing") != std::string::npos);
    CHECK(fixture.titles() == std::vector<std::string>{"found"});
}

TEST_CASE("scanFolders: stops when asked to")
{
    Fixture fixture;
    fixture.write("a.mp3");
    const std::atomic<bool> stop{true};

    CHECK(fixture.scan(stop).empty());

    CHECK(fixture.store.trackCount().value() == 0);
}
