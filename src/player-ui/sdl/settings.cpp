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
        constexpr std::string_view header           = "djw-settings 1";
        constexpr std::string_view latency_label    = "buffer-frames ";
        constexpr std::string_view device_label     = "output-device ";
        constexpr std::string_view channels_label   = "output-channels ";
        constexpr std::string_view controller_label = "controller ";
        /// Between a port and its mapping on a controller line: names may hold spaces, never a tab
        constexpr char separator = '\t';

        bool noTab(std::string_view text)
        {
            return text.find(separator) == std::string_view::npos && text.find('\n') == std::string_view::npos;
        }
    } // namespace

    std::string formatSettings(const Settings &settings)
    {
        std::ostringstream out;
        out << header << '\n';
        out << latency_label << settings.buffer_frames << '\n';
        if (!settings.output_device.empty() && noTab(settings.output_device))
        {
            out << device_label << settings.output_device << '\n';
        }
        out << channels_label << (settings.headphone_output ? 4 : 2) << '\n';
        for (const auto &[port, mapping] : settings.controllers)
        {
            if (!port.empty() && noTab(port) && noTab(mapping))
            {
                out << controller_label << port << separator << mapping << '\n';
            }
        }
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
            if (line.starts_with(device_label))
            {
                settings.output_device = line.substr(device_label.size());
                continue;
            }
            if (line.starts_with(channels_label))
            {
                settings.headphone_output = line.substr(channels_label.size()) == "4";
                continue;
            }
            if (line.starts_with(controller_label))
            {
                const std::string_view rest = std::string_view{line}.substr(controller_label.size());
                const std::size_t      tab  = rest.find(separator);
                if (tab != std::string_view::npos && tab > 0)
                {
                    settings.controllers[std::string{rest.substr(0, tab)}] = std::string{rest.substr(tab + 1)};
                }
                continue;
            }
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
