// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_API_LIBRARY_HPP
#define DJW_CORE_API_LIBRARY_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

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
    };

    /// Columns the track list can be sorted by
    enum class LibraryColumn : std::uint8_t
    {
        Artist,
        Title,
        Genre,
        Year,
        Duration
    };

    /**
     * A search in the library.
     *
     * text searches as you type: every word must appear at the start of a word in artist, title, album, genre or file
     * name. A word with the wildcards * (any characters) or ? (one character) is searched anywhere in those fields.
     *
     * The column filters narrow down further. Without wildcards a filter matches when the column contains it; with
     * wildcards the pattern must match the whole value ("Daft*": starts with Daft). Empty filters and text match
     * everything. Case and accents are ignored everywhere for Latin script ("beyonce" finds "Beyoncé").
     */
    struct LibraryQuery
    {
            std::string text{};
            std::string artist{};
            std::string title{};
            std::string genre{};
            std::string year{};

            LibraryColumn sort_by    = LibraryColumn::Artist;
            bool          descending = false;
            /// At most this many tracks
            std::size_t limit = 1'000;

            bool operator==(const LibraryQuery &) const = default;
    };

    /// Progress of the folder scan; counts are those of the current or last scan.
    struct LibraryScanStatus
    {
            bool        running     = false;
            std::size_t files_found = 0;
            /// New or changed files whose tags were read; unchanged files are skipped
            std::size_t files_updated = 0;
            std::size_t tracks_total  = 0;
            /// Last error of the scan (e.g. a folder that cannot be read), empty if none
            std::string error{};
    };

    /**
     * The music library: the folders it covers, their tracks with tags, and a search over them.
     *
     * Scanning runs on the library's own thread; the methods return at once. search() runs on the caller's thread
     * and sees what the scan has stored so far. All methods are called from one thread, the frontend's.
     */
    class Library
    {
        public:
            Library()                           = default;
            Library(const Library &)            = delete;
            Library &operator=(const Library &) = delete;
            Library(Library &&)                 = delete;
            Library &operator=(Library &&)      = delete;
            virtual ~Library()                  = default;

            /// Adds a folder to the library (kept in the database) and scans it, including subfolders.
            virtual std::expected<void, std::string> addFolder(const std::filesystem::path &folder) = 0;

            /// Scans all folders of the library again; only new and changed files (mtime, size) are read.
            virtual void rescan() = 0;

            [[nodiscard]] virtual std::vector<std::filesystem::path> folders() const    = 0;
            [[nodiscard]] virtual LibraryScanStatus                  scanStatus() const = 0;

            /**
             * The tracks matching query, sorted by its column (ties by artist and title; empty values last in either
             * direction), at most query.limit of them.
             */
            [[nodiscard]] virtual std::expected<std::vector<LibraryTrack>, std::string>
            search(const LibraryQuery &query) const = 0;
    };

    struct LibraryConfig
    {
            /// The database file; created with its folder if missing
            std::filesystem::path database_file;
    };

    /// Opens (or creates) the library database. Implemented in core/library.
    std::expected<std::unique_ptr<Library>, std::string> openLibrary(const LibraryConfig &config);
} // namespace djw::core::api

#endif // DJW_CORE_API_LIBRARY_HPP
