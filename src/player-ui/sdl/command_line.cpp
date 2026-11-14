// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "command_line.hpp"

#include <string_view>

namespace djw::player_ui::sdl
{
    std::expected<CommandLine, std::string> parseCommandLine(std::span<const char *const> arguments)
    {
        CommandLine result;
        for (std::size_t index = 0; index < arguments.size(); ++index)
        {
            const std::string_view argument{arguments[index]};
            if (argument == "--version")
            {
                result.version = true;
            }
            else if (argument == "--headphones")
            {
                result.headphones = true;
            }
            else if (argument == "--add-folder")
            {
                if (++index == arguments.size())
                {
                    return std::unexpected{std::string{"--add-folder needs a folder"}};
                }
                result.add_folders.emplace_back(arguments[index]);
            }
            else if (argument.starts_with("--"))
            {
                return std::unexpected{"unknown option " + std::string{argument}};
            }
            else if (result.file)
            {
                return std::unexpected{"more than one audio file: " + std::string{argument}};
            }
            else
            {
                result.file = std::filesystem::path{argument};
            }
        }
        return result;
    }
} // namespace djw::player_ui::sdl
