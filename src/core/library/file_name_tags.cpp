// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library/file_name_tags.hpp"

#include "library/utf8_path.hpp"

#include <algorithm>
#include <string_view>
#include <utility>

namespace djw::core::library
{
    namespace
    {
        std::string trimmed(std::string_view text)
        {
            const auto first = text.find_first_not_of(' ');
            if (first == std::string_view::npos)
            {
                return {};
            }
            const auto last = text.find_last_not_of(' ');
            return std::string{text.substr(first, last - first + 1)};
        }
    } // namespace

    TrackTags tagsFromFileName(const std::filesystem::path &file)
    {
        std::string name = toUtf8(file.stem());
        std::ranges::replace(name, '_', ' ');

        constexpr std::string_view separator = " - ";
        TrackTags                  tags;
        if (const auto split = name.find(separator); split != std::string::npos)
        {
            tags.artist = trimmed(std::string_view{name}.substr(0, split));
            tags.title  = trimmed(std::string_view{name}.substr(split + separator.size()));
        }
        else
        {
            tags.title = trimmed(name);
        }
        return tags;
    }

    TrackTags withFileNameFallback(TrackTags tags, const std::filesystem::path &file)
    {
        if (!tags.artist.empty() && !tags.title.empty())
        {
            return tags;
        }
        TrackTags from_name = tagsFromFileName(file);
        if (tags.title.empty())
        {
            tags.title = std::move(from_name.title);
        }
        if (tags.artist.empty())
        {
            tags.artist = std::move(from_name.artist);
        }
        return tags;
    }
} // namespace djw::core::library
