// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_LIBRARY_TRACK_TAGS_HPP
#define DJW_CORE_LIBRARY_TRACK_TAGS_HPP

#include <filesystem>
#include <functional>
#include <string>

namespace djw::core::library
{
    /// What the library knows about a file's content. Missing values are empty (year: 0).
    struct TrackTags
    {
            std::string artist{};
            std::string title{};
            std::string album{};
            std::string genre{};
            int         year             = 0;
            double      duration_seconds = 0.0;

            bool operator==(const TrackTags &) const = default;
    };

    /// Reads the tags of a file; injected into the scanner, so tests can count and fake tag reads
    using TagReader = std::function<TrackTags(const std::filesystem::path &file)>;
} // namespace djw::core::library

#endif // DJW_CORE_LIBRARY_TRACK_TAGS_HPP
