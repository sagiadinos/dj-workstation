// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_COMMAND_LINE_HPP
#define DJW_PLAYER_UI_SDL_COMMAND_LINE_HPP

#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace djw::player_ui::sdl
{
    /// What the player was started with
    struct CommandLine
    {
            /// Master on output channels 1/2, headphones on 3/4
            bool headphones = false;
            /// Folders to add to the library (and scan) at start
            std::vector<std::filesystem::path> add_folders;
            /// A file to load into the deck at start
            std::optional<std::filesystem::path> file;
    };

    inline constexpr const char *usage = "usage: djw [--headphones] [--add-folder <folder>]... [<audio file>]\n"
                                         "  --headphones          master on output channels 1/2, headphones on 3/4\n"
                                         "                        (needs a 4-channel audio device)\n"
                                         "  --add-folder <folder> add a music folder to the library and scan it\n";

    /// Parses the arguments after the program name; an error names the argument that is wrong
    std::expected<CommandLine, std::string> parseCommandLine(std::span<const char *const> arguments);
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_COMMAND_LINE_HPP
