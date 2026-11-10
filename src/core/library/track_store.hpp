// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_LIBRARY_TRACK_STORE_HPP
#define DJW_CORE_LIBRARY_TRACK_STORE_HPP

#include "api/library.hpp"
#include "library/sqlite_database.hpp"
#include "library/track_tags.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace djw::core::library
{
    /// What tells a changed file from an unchanged one
    struct FileStamp
    {
            /// Last write time in the ticks of std::filesystem's clock
            std::int64_t   modified = 0;
            std::uintmax_t size     = 0;

            bool operator==(const FileStamp &) const = default;
    };

    /**
     * The library tables on one database connection: folders, tracks and the full text index over the tracks.
     *
     * The schema is created on open; the full text index follows the tracks table through triggers. One TrackStore
     * per thread; several can share the database file.
     */
    class TrackStore
    {
        public:
            /// Database schema version, stored as PRAGMA user_version
            static constexpr std::int64_t schema_version = 2;

            static std::expected<TrackStore, std::string> open(const std::filesystem::path &database_file);

            std::expected<void, std::string> addFolder(const std::filesystem::path &folder);
            [[nodiscard]] std::expected<std::vector<std::filesystem::path>, std::string> folders();

            /// The stamp stored for a file, std::nullopt if the file is not in the library
            std::expected<std::optional<FileStamp>, std::string> stamp(const std::filesystem::path &file);
            /// Adds the file or updates it
            std::expected<void, std::string> store(const std::filesystem::path &file, const FileStamp &stamp,
                                                   const TrackTags &tags);

            std::expected<std::vector<api::LibraryTrack>, std::string> search(const api::LibraryQuery &query);
            std::expected<std::size_t, std::string>                    trackCount();

            std::expected<void, std::string> beginTransaction() { return database.execute("BEGIN"); }
            std::expected<void, std::string> commitTransaction() { return database.execute("COMMIT"); }

        private:
            struct Statements
            {
                    Statement add_folder;
                    Statement folders;
                    Statement stamp;
                    Statement store;
                    Statement count;
            };

            TrackStore(SqliteDatabase database, Statements statements) noexcept
                : database{std::move(database)}, statements{std::move(statements)}
            {
            }

            static std::expected<void, std::string>       createSchema(SqliteDatabase &database);
            static std::expected<void, std::string>       migrateToVersion2(SqliteDatabase &database);
            static std::expected<Statements, std::string> prepareStatements(SqliteDatabase &database);

            SqliteDatabase database;
            Statements     statements;
    };
} // namespace djw::core::library

#endif // DJW_CORE_LIBRARY_TRACK_STORE_HPP
