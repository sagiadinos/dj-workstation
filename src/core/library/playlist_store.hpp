// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_LIBRARY_PLAYLIST_STORE_HPP
#define DJW_CORE_LIBRARY_PLAYLIST_STORE_HPP

#include "api/playlists.hpp"
#include "library/sqlite_database.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace djw::core::library
{
    /**
     * The playlist tables on a connection a TrackStore created (it owns the schema). Entries are kept with dense
     * positions; every change of the order rewrites a playlist's entries in one transaction, which is simple and
     * fast enough for playlists of a few thousand tracks.
     */
    class PlaylistStore
    {
        public:
            /// The connection must outlive the store
            static std::expected<PlaylistStore, std::string> open(SqliteDatabase &database);

            std::expected<std::vector<api::Playlist>, std::string>     list();
            std::expected<std::vector<api::LibraryTrack>, std::string> tracks(std::int64_t playlist_id);
            /// The track ids of a playlist in their order
            std::expected<std::vector<std::int64_t>, std::string>   entries(std::int64_t playlist_id);
            std::expected<std::optional<std::int64_t>, std::string> idOf(std::string_view name);

            std::expected<std::int64_t, std::string> create(std::string_view name);
            std::expected<void, std::string>         rename(std::int64_t playlist_id, std::string_view name);
            std::expected<void, std::string>         remove(std::int64_t playlist_id);
            /// Replaces the entries of a playlist with track_ids, in this order
            std::expected<void, std::string> setEntries(std::int64_t                     playlist_id,
                                                        const std::vector<std::int64_t> &track_ids);

        private:
            struct Statements
            {
                    Statement list;
                    Statement tracks;
                    Statement entries;
                    Statement id_of;
                    Statement create;
                    Statement rename;
                    Statement remove;
                    Statement clear;
                    Statement insert;
            };

            PlaylistStore(SqliteDatabase &database, Statements statements) noexcept
                : database{&database}, statements{std::move(statements)}
            {
            }

            SqliteDatabase *database;
            Statements      statements;
    };
} // namespace djw::core::library

#endif // DJW_CORE_LIBRARY_PLAYLIST_STORE_HPP
