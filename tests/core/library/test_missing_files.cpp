// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library/missing_files.hpp"

#include "temp_dir.hpp"

#include <doctest/doctest.h>

#include <fstream>

using namespace djw::core::library;
namespace api = djw::core::api;

namespace
{
    struct Fixture
    {
            Fixture() : store{TrackStore::open(data.path() / "library.db").value()} {}

            std::filesystem::path add(const std::string &name)
            {
                const auto file = music.path() / name;
                std::ofstream{file} << "x";
                REQUIRE(store.store(file, {.modified = 1, .size = 1}, {.title = name}).has_value());
                return file;
            }

            std::vector<std::string> missingTitles()
            {
                std::vector<std::string> titles;
                // Held in a variable: GCC 13 and Clang 18 do not yet extend temporaries in range-for (C++23)
                const auto found = store.search({.only_missing = true}).value();
                for (const auto &track : found)
                {
                    CHECK(track.missing);
                    titles.push_back(track.title);
                }
                return titles;
            }

            static inline const std::atomic<bool> no_stop{false};

            djw::tests::TempDir music;
            djw::tests::TempDir data;
            TrackStore          store;
    };
} // namespace

TEST_CASE("checkMissingFiles: marks tracks whose file is gone, the others stay")
{
    Fixture    fixture;
    const auto gone = fixture.add("gone.mp3");
    fixture.add("here.mp3");
    std::filesystem::remove(gone);

    CHECK(checkMissingFiles(fixture.store, Fixture::no_stop).empty());

    CHECK(fixture.missingTitles() == std::vector<std::string>{"gone.mp3"});
    CHECK(fixture.store.missingCount().value() == 1);
    CHECK(fixture.store.trackCount().value() == 2);
}

TEST_CASE("checkMissingFiles: a file that is back loses its mark")
{
    Fixture    fixture;
    const auto file = fixture.add("away.mp3");
    std::filesystem::rename(file, fixture.music.path() / "elsewhere");
    REQUIRE(checkMissingFiles(fixture.store, Fixture::no_stop).empty());
    REQUIRE(fixture.store.missingCount().value() == 1);

    std::filesystem::rename(fixture.music.path() / "elsewhere", file);
    CHECK(checkMissingFiles(fixture.store, Fixture::no_stop).empty());

    CHECK(fixture.store.missingCount().value() == 0);
}

TEST_CASE("checkMissingFiles: a folder in place of the file counts as missing")
{
    Fixture    fixture;
    const auto file = fixture.add("a.mp3");
    std::filesystem::remove(file);
    std::filesystem::create_directory(file);

    CHECK(checkMissingFiles(fixture.store, Fixture::no_stop).empty());

    CHECK(fixture.store.missingCount().value() == 1);
}

TEST_CASE("TrackStore: removeMissing deletes the marked tracks only, also from the search")
{
    Fixture    fixture;
    const auto gone = fixture.add("gone.mp3");
    fixture.add("here.mp3");
    std::filesystem::remove(gone);
    REQUIRE(checkMissingFiles(fixture.store, Fixture::no_stop).empty());

    REQUIRE(fixture.store.removeMissing().has_value());

    CHECK(fixture.store.trackCount().value() == 1);
    CHECK(fixture.store.search({.text = "gone"}).value().empty());
    CHECK(fixture.store.search({.text = "here"}).value().size() == 1);
}

TEST_CASE("checkMissingFiles: stops when asked to")
{
    Fixture    fixture;
    const auto gone = fixture.add("gone.mp3");
    std::filesystem::remove(gone);
    const std::atomic<bool> stop{true};

    CHECK(checkMissingFiles(fixture.store, stop).empty());

    CHECK(fixture.store.missingCount().value() == 0);
}
