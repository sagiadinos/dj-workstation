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
#include <memory>
#include <optional>
#include <span>
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

    /// A track's file as the dead link check sees it
    struct StoredFile
    {
            std::int64_t          track_id = 0;
            std::filesystem::path path;
            bool                  missing = false;
    };

    /// The columns readTrack expects, for SELECTs over the tracks table with the alias t
    [[nodiscard]] std::string_view trackColumns() noexcept;
    /// A track from a row with the columns of trackColumns()
    [[nodiscard]] api::LibraryTrack readTrack(const Statement &row);

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
            static constexpr std::int64_t schema_version = 6;

            static std::expected<TrackStore, std::string> open(const std::filesystem::path &database_file);

            std::expected<void, std::string> addFolder(const std::filesystem::path &folder);
            [[nodiscard]] std::expected<std::vector<std::filesystem::path>, std::string> folders();

            /// The stamp stored for a file, std::nullopt if the file is not in the library
            std::expected<std::optional<FileStamp>, std::string> stamp(const std::filesystem::path &file);
            /// Adds the file or updates it
            /// With keep_analysis the stored analysis stays (the audio did not change, e.g. after a tag edit);
            /// otherwise it is dropped for a file that was stored before.
            std::expected<void, std::string> store(const std::filesystem::path &file, const FileStamp &stamp,
                                                   const TrackTags &tags, bool keep_analysis = false);

            std::expected<std::vector<api::LibraryTrack>, std::string> search(const api::LibraryQuery &query);
            std::expected<std::size_t, std::string>                    trackCount();
            std::expected<std::size_t, std::string>                    missingCount();
            std::expected<std::vector<StoredFile>, std::string>        files();
            std::expected<void, std::string>                           setMissing(std::int64_t track_id, bool missing);
            /// Stores the cue point of a track in seconds; std::nullopt removes it
            std::expected<void, std::string> setCue(std::int64_t track_id, std::optional<double> seconds);
            /// Stores the result of the analysis: the tempo (std::nullopt: none found) and the overview waveform
            std::expected<void, std::string> setAnalysis(std::int64_t track_id, std::optional<double> bpm,
                                                         std::span<const std::uint8_t> waveform);
            /// The overview waveform of a track, empty if it is not analyzed
            std::expected<std::vector<std::uint8_t>, std::string> waveform(std::int64_t track_id);
            /// Whether a track has been analyzed; false for an unknown track
            std::expected<bool, std::string> isAnalyzed(std::int64_t track_id);
            /// The tracks not analyzed yet whose file is not missing
            std::expected<std::vector<StoredFile>, std::string> unanalyzed();
            /// Deletes all tracks marked missing
            std::expected<void, std::string> removeMissing();

            /// The track of a file, std::nullopt if the file is not in the library
            std::expected<std::optional<std::int64_t>, std::string> trackId(const std::filesystem::path &file);

            /// The file of a track, std::nullopt for an unknown id
            std::expected<std::optional<std::filesystem::path>, std::string> trackPath(std::int64_t track_id);

            std::expected<void, std::string> beginTransaction() { return database->execute("BEGIN"); }
            std::expected<void, std::string> commitTransaction() { return database->execute("COMMIT"); }

            /// The connection, e.g. for the playlists on it; it stays where it is when the store is moved
            [[nodiscard]] SqliteDatabase &connection() noexcept { return *database; }

        private:
            struct Statements
            {
                    Statement add_folder;
                    Statement folders;
                    Statement stamp;
                    Statement store;
                    Statement count;
                    Statement path;
                    Statement files;
                    Statement set_missing;
                    Statement missing_count;
                    Statement remove_missing;
                    Statement id_of_path;
                    Statement set_cue;
                    Statement set_analysis;
                    Statement set_waveform;
                    Statement drop_waveform;
                    Statement waveform;
                    Statement is_analyzed;
                    Statement unanalyzed;
            };

            TrackStore(std::unique_ptr<SqliteDatabase> database, Statements statements) noexcept
                : database{std::move(database)}, statements{std::move(statements)}
            {
            }

            static std::expected<void, std::string>       createSchema(SqliteDatabase &database);
            static std::expected<void, std::string>       migrate(SqliteDatabase &database, std::int64_t from_version);
            static std::expected<void, std::string>       migrateToVersion2(SqliteDatabase &database);
            static std::expected<Statements, std::string> prepareStatements(SqliteDatabase &database);

            std::unique_ptr<SqliteDatabase> database;
            Statements                      statements;
    };
} // namespace djw::core::library

#endif // DJW_CORE_LIBRARY_TRACK_STORE_HPP
