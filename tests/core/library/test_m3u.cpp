// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library/m3u.hpp"

#include <doctest/doctest.h>

using namespace djw::core::library;
namespace api = djw::core::api;
using std::filesystem::path;

TEST_CASE("parseM3u: paths in order, comments and empty lines skipped, relative paths from the playlist folder")
{
    const auto paths =
        parseM3u("#EXTM3U\n#EXTINF:245,A - B\n/music/a.mp3\n\nsub/b.flac\n# comment\n../c.wav\n", "/lists/house");
    CHECK(paths == std::vector<path>{"/music/a.mp3", "/lists/house/sub/b.flac", "/lists/c.wav"});
}

TEST_CASE("parseM3u: Windows line ends, a byte order mark and surrounding blanks are accepted")
{
    const auto paths = parseM3u("\xEF\xBB\xBF#EXTM3U\r\n  /music/a.mp3  \r\n/music/b.mp3", "/lists");
    CHECK(paths == std::vector<path>{"/music/a.mp3", "/music/b.mp3"});
}

TEST_CASE("parseM3u: file URLs are decoded, UTF-8 paths stay as they are")
{
    const auto paths =
        parseM3u("file:///music/Die%20%C3%84rzte.mp3\nfile://localhost/music/b.mp3\n/music/Beyoncé.flac\n", "/");
    CHECK(paths == std::vector<path>{path{u8"/music/Die Ärzte.mp3"}, "/music/b.mp3", path{u8"/music/Beyoncé.flac"}});
}

TEST_CASE("parseM3u: a broken percent escape stays as it is")
{
    CHECK(parseM3u("file:///music/100%.mp3\n", "/") == std::vector<path>{"/music/100%.mp3"});
}

TEST_CASE("formatM3u: extended M3U with rounded seconds, artist - title and the path")
{
    const std::vector<api::LibraryTrack> tracks{
        {.path = "/music/a.mp3", .artist = "Daft Punk", .title = "One More Time", .duration_seconds = 320.6},
        {.path = path{u8"/music/Ä.flac"}, .title = "No Artist", .duration_seconds = 0},
    };
    CHECK(formatM3u(tracks) == "#EXTM3U\n"
                               "#EXTINF:321,Daft Punk - One More Time\n/music/a.mp3\n"
                               "#EXTINF:0,No Artist\n/music/Ä.flac\n");
    CHECK(formatM3u({}) == "#EXTM3U\n");
}
