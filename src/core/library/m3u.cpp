// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library/m3u.hpp"

#include "library/utf8_path.hpp"

#include <cmath>
#include <optional>

namespace djw::core::library
{
    namespace
    {
        constexpr std::string_view byte_order_mark = "\xEF\xBB\xBF";
        constexpr std::string_view file_scheme     = "file://";

        std::optional<int> hexValue(char digit)
        {
            if (digit >= '0' && digit <= '9')
            {
                return digit - '0';
            }
            if (digit >= 'a' && digit <= 'f')
            {
                return digit - 'a' + 10;
            }
            if (digit >= 'A' && digit <= 'F')
            {
                return digit - 'A' + 10;
            }
            return std::nullopt;
        }

        /// %XX escapes of a URL decoded; a broken escape stays as it is
        std::string percentDecoded(std::string_view text)
        {
            std::string decoded;
            for (std::size_t index = 0; index < text.size(); ++index)
            {
                if (text[index] == '%' && index + 2 < text.size())
                {
                    const auto high = hexValue(text[index + 1]);
                    const auto low  = hexValue(text[index + 2]);
                    if (high && low)
                    {
                        decoded += static_cast<char>((*high * 16) + *low);
                        index += 2;
                        continue;
                    }
                }
                decoded += text[index];
            }
            return decoded;
        }

        std::string_view trimmed(std::string_view line)
        {
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
            {
                line.remove_suffix(1);
            }
            while (!line.empty() && (line.front() == ' ' || line.front() == '\t'))
            {
                line.remove_prefix(1);
            }
            return line;
        }

        std::filesystem::path entryPath(std::string_view line, const std::filesystem::path &base_folder)
        {
            if (line.starts_with(file_scheme))
            {
                std::string_view rest = line.substr(file_scheme.size());
                // file://host/path: only the local host (empty or localhost) is a path on this machine
                if (rest.starts_with("localhost/"))
                {
                    rest.remove_prefix(std::string_view{"localhost"}.size());
                }
                return fromUtf8(percentDecoded(rest));
            }
            const std::filesystem::path path = fromUtf8(line);
            return path.is_absolute() ? path : base_folder / path;
        }
    } // namespace

    std::vector<std::filesystem::path> parseM3u(std::string_view content, const std::filesystem::path &base_folder)
    {
        if (content.starts_with(byte_order_mark))
        {
            content.remove_prefix(byte_order_mark.size());
        }
        std::vector<std::filesystem::path> paths;
        while (!content.empty())
        {
            const auto             end  = content.find('\n');
            const std::string_view line = trimmed(content.substr(0, end));
            content.remove_prefix(end == std::string_view::npos ? content.size() : end + 1);
            if (!line.empty() && !line.starts_with('#'))
            {
                paths.push_back(entryPath(line, base_folder).lexically_normal());
            }
        }
        return paths;
    }

    std::string formatM3u(const std::vector<api::LibraryTrack> &tracks)
    {
        std::string text = "#EXTM3U\n";
        for (const api::LibraryTrack &track : tracks)
        {
            const std::string name = track.artist.empty() ? track.title : track.artist + " - " + track.title;
            text += "#EXTINF:" + std::to_string(std::lround(track.duration_seconds)) + "," + name + "\n";
            text += toUtf8(track.path) + "\n";
        }
        return text;
    }
} // namespace djw::core::library
