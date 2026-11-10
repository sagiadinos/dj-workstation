// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_API_PLAYLISTS_HPP
#define DJW_CORE_API_PLAYLISTS_HPP

#include "api/library_track.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace djw::core::api
{
    struct Playlist
    {
            std::int64_t id = 0;
            std::string  name{};
            std::size_t  track_count = 0;
    };

    /// Outcome of the last M3U import
    struct PlaylistImport
    {
            std::string playlist{};
            std::size_t tracks = 0;
            /// Entries whose file does not exist; they are left out
            std::size_t not_found = 0;

            bool operator==(const PlaylistImport &) const = default;
    };

    /**
     * Saved playlists of the library: an ordered list of library tracks each, with M3U/M3U8 import and export.
     *
     * Reading runs on the caller's thread. Every change runs on the library thread like tag edits and returns at once;
     * the library's revision goes up when it is done, errors go to LibraryStatus::write_error. Entries are addressed
     * by their position in the list. Removing a track from the library removes it from its playlists.
     */
    class Playlists
    {
        public:
            Playlists()                             = default;
            Playlists(const Playlists &)            = delete;
            Playlists &operator=(const Playlists &) = delete;
            Playlists(Playlists &&)                 = delete;
            Playlists &operator=(Playlists &&)      = delete;
            virtual ~Playlists()                    = default;

            /// All playlists, ordered by name
            [[nodiscard]] virtual std::expected<std::vector<Playlist>, std::string> list() const = 0;
            /// The tracks of a playlist in their order; empty for an unknown playlist
            [[nodiscard]] virtual std::expected<std::vector<LibraryTrack>, std::string>
            tracks(std::int64_t playlist_id) const = 0;

            /// Names are unique; a name that is taken is an error
            virtual void create(std::string name)                           = 0;
            virtual void rename(std::int64_t playlist_id, std::string name) = 0;
            virtual void remove(std::int64_t playlist_id)                   = 0;
            /// Appends the tracks at the end, in the given order
            virtual void add(std::int64_t playlist_id, std::vector<std::int64_t> track_ids) = 0;
            virtual void removeEntry(std::int64_t playlist_id, std::size_t index)           = 0;
            /// Moves the entry at from to position to; the others close up
            virtual void moveEntry(std::int64_t playlist_id, std::size_t from, std::size_t to) = 0;

            /**
             * Reads an M3U/M3U8 file into a new playlist named after the file (with a number if the name is taken).
             * Paths may be relative to the file. Files not yet in the library are added with their tags, files that
             * do not exist are counted and left out, see LibraryStatus::last_import.
             */
            virtual void importM3u(std::filesystem::path file) = 0;
            /// Writes the playlist as an extended M3U file in UTF-8, with absolute paths
            virtual void exportM3u(std::int64_t playlist_id, std::filesystem::path file) = 0;
    };
} // namespace djw::core::api

#endif // DJW_CORE_API_PLAYLISTS_HPP
