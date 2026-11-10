// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_LIBRARY_PLAYLIST_FILES_HPP
#define DJW_CORE_LIBRARY_PLAYLIST_FILES_HPP

#include "api/playlists.hpp"
#include "library/playlist_store.hpp"
#include "library/track_store.hpp"
#include "library/track_tags.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>

namespace djw::core::library
{
    /**
     * Reads an M3U/M3U8 file into a new playlist named after the file; a taken name gets " (2)", " (3)", ...
     * Entries already in the library are used as they are, existing audio files not yet in it are added with their
     * tags (read_tags), and entries whose file does not exist are counted and left out.
     */
    std::expected<api::PlaylistImport, std::string> importM3uFile(const std::filesystem::path &file, TrackStore &tracks,
                                                                  PlaylistStore &playlists, const TagReader &read_tags);

    /// Writes a playlist as an extended M3U file in UTF-8 (see formatM3u); an error names the file
    std::expected<void, std::string> exportM3uFile(std::int64_t playlist_id, const std::filesystem::path &file,
                                                   PlaylistStore &playlists);
} // namespace djw::core::library

#endif // DJW_CORE_LIBRARY_PLAYLIST_FILES_HPP
