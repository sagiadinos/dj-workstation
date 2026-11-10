// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_LIBRARY_M3U_HPP
#define DJW_CORE_LIBRARY_M3U_HPP

#include "api/library_track.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace djw::core::library
{
    /**
     * The files an M3U/M3U8 playlist lists, in order. Comment and directive lines (#...) and empty lines are skipped,
     * Windows line ends and a UTF-8 byte order mark are accepted, file:// URLs are decoded, and relative paths are
     * taken relative to base_folder (the folder of the playlist file). The text is read as UTF-8.
     */
    std::vector<std::filesystem::path> parseM3u(std::string_view content, const std::filesystem::path &base_folder);

    /// An extended M3U playlist in UTF-8: #EXTM3U, then #EXTINF (seconds, "artist - title") and the path per track
    std::string formatM3u(const std::vector<api::LibraryTrack> &tracks);
} // namespace djw::core::library

#endif // DJW_CORE_LIBRARY_M3U_HPP
