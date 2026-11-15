// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library/tag_reader.hpp"

#include "temp_dir.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

using djw::core::library::readTags;
using djw::core::library::TrackTags;

namespace
{
    const std::filesystem::path audio_dir{DJW_TEST_AUDIO_DIR};

    /// A field of an ID3v1 tag: the text, padded with zeros to its size
    std::string field(std::string_view text, std::size_t size)
    {
        std::string padded{text.substr(0, size)};
        padded.resize(size, '\0');
        return padded;
    }

    /// The test MP3 without its ID3v2 tag, with an ID3v1 tag (128 bytes at the end) and, if wanted, an ID3v2 tag
    /// in front again
    void writeId3v1Mp3(const std::filesystem::path &file, std::string_view comment, std::uint8_t genre,
                       bool keep_id3v2 = false)
    {
        std::ifstream     in{audio_dir / "sine_440hz_48k_stereo.mp3", std::ios::binary};
        const std::string mp3{std::istreambuf_iterator<char>{in}, {}};
        REQUIRE(mp3.starts_with("ID3"));
        // The ID3v2 size: four bytes of seven bits each, after the ten bytes of the header
        std::size_t size = 0;
        for (std::size_t at = 6; at < 10; ++at)
        {
            size = (size << 7U) | (static_cast<std::size_t>(static_cast<unsigned char>(mp3[at])) & 0x7FU);
        }
        const std::string frames = keep_id3v2 ? mp3 : mp3.substr(10 + size);
        const std::string tag    = "TAG" + field("V1 Title", 30) + field("V1 Artist", 30) + field("V1 Album", 30) +
                                   "1998" + field(comment, 30) + std::string(1, static_cast<char>(genre));
        std::ofstream{file, std::ios::binary} << frames << tag;
    }
} // namespace

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

TEST_CASE("readTags: an MP3 with only an ID3v1 tag takes its genre from the comment")
{
    const djw::tests::TempDir folder;
    const auto                file = folder.path() / "old.mp3";
    writeId3v1Mp3(file, "Italo Disco", 0); // genre 0: Blues

    const TrackTags tags = readTags(file);
    CHECK(tags.artist == "V1 Artist");
    CHECK(tags.title == "V1 Title");
    CHECK(tags.year == 1998);
    CHECK(tags.genre == "Italo Disco");
}

TEST_CASE("readTags: an ID3v1 tag with an empty comment keeps its genre; with an ID3v2 tag the comment is no genre")
{
    const djw::tests::TempDir folder;
    const auto                empty = folder.path() / "empty.mp3";
    writeId3v1Mp3(empty, "", 35); // 35: House
    CHECK(readTags(empty).genre == "House");

    const auto both = folder.path() / "both.mp3";
    writeId3v1Mp3(both, "Italo Disco", 0, true);
    CHECK(readTags(both).genre != "Italo Disco"); // the ID3v2 tag decides
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
