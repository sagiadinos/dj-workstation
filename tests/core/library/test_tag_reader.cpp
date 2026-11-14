// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library/tag_reader.hpp"

#include "temp_dir.hpp"

#include <doctest/doctest.h>

#include <fstream>
#include <iterator>
#include <string>

using djw::core::library::readTags;
using djw::core::library::TrackTags;

namespace
{
    const std::filesystem::path audio_dir{DJW_TEST_AUDIO_DIR};
}

TEST_CASE("readTags: ID3v2 tags and duration of an MP3 file")
{
    const TrackTags tags = readTags(audio_dir / "tagged_id3v2.mp3");
    CHECK(tags.artist == "Tagged Artist");
    CHECK(tags.title == "Tagged Title");
    CHECK(tags.album == "Test Album");
    CHECK(tags.genre == "House");
    CHECK(tags.year == 2024);
    CHECK(tags.duration_seconds == doctest::Approx(0.25).epsilon(0.2)); // MP3 frames round the length
}

TEST_CASE("readTags: Vorbis comments of a FLAC file, in UTF-8")
{
    const TrackTags tags = readTags(audio_dir / "tagged_vorbis.flac");
    CHECK(tags.artist == "Beyoncé");
    CHECK(tags.title == "Ünïcode Title");
    CHECK(tags.genre == "Pop");
    CHECK(tags.year == 2019);
    CHECK(tags.duration_seconds == doctest::Approx(0.25));
}

TEST_CASE("readTags: a file without tags gets artist and title from its name")
{
    const djw::tests::TempDir folder;
    const auto                file = folder.path() / "Name Artist - Name Title.wav";
    std::filesystem::copy_file(audio_dir / "sine_440hz_48k_stereo.wav", file);

    const TrackTags tags = readTags(file);

    CHECK(tags.artist == "Name Artist");
    CHECK(tags.title == "Name Title");
    CHECK(tags.duration_seconds == doctest::Approx(0.25));
}

TEST_CASE("readTags: a file TagLib cannot read still gets a title from its name")
{
    const djw::tests::TempDir folder;
    const auto                file = folder.path() / "broken.mp3";
    std::ofstream{file} << "not audio";

    const TrackTags tags = readTags(file);

    CHECK(tags.title == "broken");
    CHECK(tags.duration_seconds == 0.0);
}

TEST_CASE("readTags: an empty file still gets artist and title from its name")
{
    const djw::tests::TempDir folder;
    const auto                file = folder.path() / "Empty Artist - Empty Title.mp3";
    std::ofstream{file};

    const TrackTags tags = readTags(file);

    CHECK(tags.artist == "Empty Artist");
    CHECK(tags.title == "Empty Title");
    CHECK(tags.duration_seconds == 0.0);
}

TEST_CASE("readTags: an MP3 cut off inside its ID3v2 tag still gets artist and title from its name")
{
    const djw::tests::TempDir folder;
    const auto                file = folder.path() / "Cut Artist - Cut Title.mp3";
    {
        std::ifstream     in{audio_dir / "tagged_id3v2.mp3", std::ios::binary};
        const std::string content{std::istreambuf_iterator<char>{in}, {}};
        std::ofstream{file, std::ios::binary}.write(content.data(), 15); // the tag header and the start of a frame
    }

    const TrackTags tags = readTags(file);

    CHECK(tags.artist == "Cut Artist");
    CHECK(tags.title == "Cut Title");
}
