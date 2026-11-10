// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_API_LIBRARY_TRACK_HPP
#define DJW_CORE_API_LIBRARY_TRACK_HPP

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace djw::core::api
{
    /// A track of the library as the frontends see it. Missing tags are empty (year: 0).
    struct LibraryTrack
    {
            std::int64_t          id = 0;
            std::filesystem::path path{};
            std::string           artist{};
            std::string           title{};
            std::string           album{};
            std::string           genre{};
            int                   year             = 0;
            double                duration_seconds = 0.0;
            /// The file was not found at the last check (dead link)
            bool missing = false;
            /// The cue point the user set last, in seconds; a deck jumps there when the track is loaded (Cue-Load)
            std::optional<double> cue_seconds{};
    };
} // namespace djw::core::api

#endif // DJW_CORE_API_LIBRARY_TRACK_HPP
