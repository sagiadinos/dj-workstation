// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "settings.hpp"

#include <algorithm>
#include <charconv>
#include <sstream>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr std::string_view header        = "djw-settings 1";
        constexpr std::string_view latency_label = "buffer-frames ";
    } // namespace

    std::string formatSettings(const Settings &settings)
    {
        std::ostringstream out;
        out << header << '\n';
        out << latency_label << settings.buffer_frames << '\n';
        return out.str();
    }

    Settings parseSettings(std::string_view text)
    {
        Settings           settings;
        std::istringstream in{std::string{text}};
        std::string        line;
        if (!std::getline(in, line) || line != header)
        {
            return settings;
        }
        while (std::getline(in, line))
        {
            if (!line.starts_with(latency_label))
            {
                continue; // unknown, e.g. from a later version
            }
            const std::string_view number = std::string_view{line}.substr(latency_label.size());
            std::uint32_t          frames = 0;
            const auto [end, error]       = std::from_chars(number.data(), number.data() + number.size(), frames);
            if (error == std::errc{} && end == number.data() + number.size() &&
                std::ranges::contains(latency_choices, frames))
            {
                settings.buffer_frames = frames;
            }
        }
        return settings;
    }
} // namespace djw::player_ui::sdl
