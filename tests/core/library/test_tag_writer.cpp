// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library/tag_reader.hpp"
#include "library/tag_writer.hpp"

#include "temp_dir.hpp"

#include <doctest/doctest.h>

#include <fstream>

using namespace djw::core::library;

namespace
{
    const std::filesystem::path audio_dir{DJW_TEST_AUDIO_DIR};

    /// A copy of a test file, so the test can change it
    struct Copy
    {
            explicit Copy(const std::string &name, const std::string &as)
            {
                std::filesystem::copy_file(audio_dir / name, file);
                std::filesystem::rename(file, folder.path() / as);
                file = folder.path() / as;
            }

            djw::tests::TempDir   folder;
            std::filesystem::path file = folder.path() / "copy";
    };
} // namespace

TEST_CASE("writeTags: changes the given fields of an MP3 file (ID3v2) and keeps the others")
{
    const Copy copy{"tagged_id3v2.mp3", "a.mp3"};

    REQUIRE(writeTags(copy.file, {.artist = "New Artist", .year = 1999}).has_value());

    const TrackTags tags = readTags(copy.file);
    CHECK(tags.artist == "New Artist");
    CHECK(tags.year == 1999);
    CHECK(tags.title == "Tagged Title");
    CHECK(tags.album == "Test Album");
    CHECK(tags.genre == "House");
}

TEST_CASE("writeTags: Vorbis comments of a FLAC file, in UTF-8")
{
    const Copy copy{"tagged_vorbis.flac", "a.flac"};

    REQUIRE(writeTags(copy.file, {.title = "Größer als Ü", .genre = "Électro"}).has_value());

    const TrackTags tags = readTags(copy.file);
    CHECK(tags.title == "Größer als Ü");
    CHECK(tags.genre == "Électro");
    CHECK(tags.artist == "Beyoncé");
}

TEST_CASE("writeTags: a file without tags gets them, the audio stays playable")
{
    const Copy copy{"sine_440hz_48k_stereo.wav", "untagged.wav"};

    REQUIRE(writeTags(copy.file, {.artist = "Wave Artist", .title = "Wave Title", .album = "Wave Album"}).has_value());

    const TrackTags tags = readTags(copy.file);
    CHECK(tags.artist == "Wave Artist");
    CHECK(tags.title == "Wave Title");
    CHECK(tags.album == "Wave Album");
    CHECK(tags.duration_seconds == doctest::Approx(0.25));
}

TEST_CASE("writeTags: year 0 and empty text remove the value")
{
    const Copy copy{"tagged_id3v2.mp3", "a.mp3"};

    REQUIRE(writeTags(copy.file, {.genre = "", .year = 0}).has_value());

    const TrackTags tags = readTags(copy.file);
    CHECK(tags.genre.empty());
    CHECK(tags.year == 0);
}

TEST_CASE("writeTags: errors name the file")
{
    const djw::tests::TempDir folder;
    const auto                broken = folder.path() / "broken.mp3";
    std::ofstream{broken} << "not audio";

    const auto written = writeTags(broken, {.title = "x"});

    REQUIRE_FALSE(written.has_value());
    CHECK(written.error().find("broken.mp3") != std::string::npos);
    CHECK_FALSE(writeTags(folder.path() / "missing.mp3", {.title = "x"}).has_value());
}
