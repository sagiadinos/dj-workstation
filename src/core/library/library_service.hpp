// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_LIBRARY_LIBRARY_SERVICE_HPP
#define DJW_CORE_LIBRARY_LIBRARY_SERVICE_HPP

#include "api/library.hpp"
#include "library/folder_scanner.hpp"
#include "library/track_store.hpp"
#include "library/track_tags.hpp"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace djw::core::library
{
    /**
     * Implementation of api::Library.
     *
     * Two connections to the database: one for the caller's thread (search, folders) and one for the library thread,
     * which scans. WAL mode lets the search read while the scan writes. Scan requests that come in during a scan
     * are merged into one more scan afterwards.
     */
    class LibraryService final : public api::Library
    {
        public:
            /// Opens both connections; the library thread starts at once and waits for scan requests
            static std::expected<std::unique_ptr<LibraryService>, std::string>
            open(const std::filesystem::path &database_file, TagReader read_tags);

            LibraryService(const LibraryService &)            = delete;
            LibraryService &operator=(const LibraryService &) = delete;
            LibraryService(LibraryService &&)                 = delete;
            LibraryService &operator=(LibraryService &&)      = delete;
            /// Stops a running scan between two files and joins the library thread
            ~LibraryService() override;

            std::expected<void, std::string>                 addFolder(const std::filesystem::path &folder) override;
            void                                             rescan() override;
            [[nodiscard]] std::vector<std::filesystem::path> folders() const override;
            [[nodiscard]] api::LibraryScanStatus             scanStatus() const override;
            [[nodiscard]] std::expected<std::vector<api::LibraryTrack>, std::string>
            search(std::string_view text, std::size_t limit) const override;

        private:
            LibraryService(TrackStore reader, TrackStore writer, TagReader read_tags);

            void run();  // library thread
            void scan(); // library thread

            // Caller's thread; mutable because a search changes no state the caller sees
            mutable TrackStore reader;
            // Library thread
            TrackStore writer;
            TagReader  read_tags;

            ScanCounters      counters;
            std::atomic<bool> stopping{false};

            mutable std::mutex      mutex;
            std::condition_variable wake;
            // Guarded by mutex
            bool        scan_requested = false;
            bool        scanning       = false;
            std::size_t tracks_total   = 0;
            std::string scan_error;

            // Last, so it starts after everything it uses exists
            std::thread thread;
    };
} // namespace djw::core::library

#endif // DJW_CORE_LIBRARY_LIBRARY_SERVICE_HPP
