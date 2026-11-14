// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "command_line.hpp"

#include <doctest/doctest.h>

#include <array>

using namespace djw::player_ui::sdl;

TEST_CASE("parseCommandLine: no arguments is fine")
{
    const auto parsed = parseCommandLine({});
    REQUIRE(parsed.has_value());
    CHECK_FALSE(parsed->version);
    CHECK_FALSE(parsed->headphones);
    CHECK(parsed->add_folders.empty());
    CHECK_FALSE(parsed->file);
}

TEST_CASE("parseCommandLine: options and a file in any order, folders may repeat")
{
    const std::array<const char *, 6> arguments{"--add-folder", "/music",       "track.mp3",
                                                "--headphones", "--add-folder", "/more"};
    const auto                        parsed = parseCommandLine(arguments);
    REQUIRE(parsed.has_value());
    CHECK(parsed->headphones);
    CHECK(parsed->add_folders == std::vector<std::filesystem::path>{"/music", "/more"});
    CHECK(parsed->file == std::filesystem::path{"track.mp3"});
}

TEST_CASE("parseCommandLine: --version")
{
    const std::array<const char *, 1> arguments{"--version"};
    const auto                        parsed = parseCommandLine(arguments);
    REQUIRE(parsed.has_value());
    CHECK(parsed->version);
}

TEST_CASE("parseCommandLine: errors name the problem")
{
    const std::array<const char *, 1> missing_folder{"--add-folder"};
    CHECK(parseCommandLine(missing_folder).error().find("--add-folder") != std::string::npos);

    const std::array<const char *, 1> unknown{"--loud"};
    CHECK(parseCommandLine(unknown).error().find("--loud") != std::string::npos);

    const std::array<const char *, 2> two_files{"a.mp3", "b.mp3"};
    CHECK(parseCommandLine(two_files).error().find("b.mp3") != std::string::npos);
}
