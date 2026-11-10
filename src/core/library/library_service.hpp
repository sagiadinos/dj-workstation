// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_LIBRARY_LIBRARY_SERVICE_HPP
#define DJW_CORE_LIBRARY_LIBRARY_SERVICE_HPP

#include "api/library.hpp"
#include "api/playlists.hpp"
#include "library/folder_scanner.hpp"
#include "library/playlist_store.hpp"
#include "library/tag_writer.hpp"
#include "library/track_store.hpp"
#include "library/track_tags.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>

namespace djw::core::library
{
    /**
     * Implementation of api::Library and api::Playlists.
     *
     * Two connections to the database: one for the caller's thread (search, reading playlists) and one for the
     * library thread, which scans and does every write. WAL mode lets the caller read while the library thread
     * writes. Writes (tag edits, removals, playlist changes) are jobs in one queue, done in order; during a scan
     * between two of its files. Scan requests that come in during a scan are merged into one more scan afterwards.
     * Every scan ends with the dead link check.
     */
    class LibraryService final : public api::Library, public api::Playlists
    {
        public:
            /// Opens both connections; the library thread starts at once and waits for work
            static std::expected<std::unique_ptr<LibraryService>, std::string>
            open(const std::filesystem::path &database_file, TagReader read_tags, TagWriter write_tags);

            LibraryService(const LibraryService &)            = delete;
            LibraryService &operator=(const LibraryService &) = delete;
            LibraryService(LibraryService &&)                 = delete;
            LibraryService &operator=(LibraryService &&)      = delete;
            /// Stops a running scan between two files, does the queued writes and joins the library thread
            ~LibraryService() override;

            // api::Library
            std::expected<void, std::string> addFolder(const std::filesystem::path &folder) override;
            void                             rescan() override;
            void                             updateTags(std::int64_t track_id, api::LibraryTagChange change) override;
            void                             removeMissing() override;
            [[nodiscard]] api::Playlists    &playlists() noexcept override { return *this; }
            [[nodiscard]] std::vector<std::filesystem::path> folders() const override;
            [[nodiscard]] api::LibraryStatus                 status() const override;
            [[nodiscard]] std::expected<std::vector<api::LibraryTrack>, std::string>
            search(const api::LibraryQuery &query) const override;

            // api::Playlists
            [[nodiscard]] std::expected<std::vector<api::Playlist>, std::string> list() const override;
            [[nodiscard]] std::expected<std::vector<api::LibraryTrack>, std::string>
                 tracks(std::int64_t playlist_id) const override;
            void create(std::string name) override;
            void rename(std::int64_t playlist_id, std::string name) override;
            void remove(std::int64_t playlist_id) override;
            void add(std::int64_t playlist_id, std::vector<std::int64_t> track_ids) override;
            void removeEntry(std::int64_t playlist_id, std::size_t index) override;
            void moveEntry(std::int64_t playlist_id, std::size_t from, std::size_t to) override;
            void importM3u(std::filesystem::path file) override;
            void exportM3u(std::int64_t playlist_id, std::filesystem::path file) override;

        private:
            /// A write, run on the library thread; an error goes to write_error
            using Job = std::function<std::expected<void, std::string>()>;

            struct Connection
            {
                    TrackStore    tracks;
                    PlaylistStore playlists;
            };

            static std::expected<Connection, std::string> connect(const std::filesystem::path &database_file);

            LibraryService(Connection reader, Connection writer, TagReader read_tags, TagWriter write_tags);

            void queue(Job job);
            /// A change of a playlist's entries: reads them, lets change edit the list and writes it back
            void changeEntries(std::int64_t playlist_id, std::function<bool(std::vector<std::int64_t> &)> change);

            void run();     // library thread
            void scan();    // library thread
            void runJobs(); // library thread
            /// During a scan, between two files: commits the scan's batch, runs the jobs, opens a new batch
            void runJobsDuringScan(); // library thread
            /// Track counts after a change, under the mutex
            void                             updateCounts(); // library thread
            std::expected<void, std::string> writeTags(std::int64_t track_id, const api::LibraryTagChange &change);

            // Caller's thread; mutable because reading changes no state the caller sees
            mutable Connection reader;
            // Library thread
            Connection writer;
            TagReader  read_tags;
            TagWriter  write_tags;

            ScanCounters      counters;
            std::atomic<bool> stopping{false};

            mutable std::mutex      mutex;
            std::condition_variable wake;
            // Guarded by mutex
            bool                               scan_requested = false;
            bool                               scanning       = false;
            std::size_t                        tracks_total   = 0;
            std::size_t                        missing_total  = 0;
            std::deque<Job>                    jobs;
            std::size_t                        jobs_running = 0;
            std::uint64_t                      revision     = 0;
            std::string                        scan_error;
            std::string                        write_error;
            std::optional<api::PlaylistImport> last_import;

            // Last, so it starts after everything it uses exists
            std::thread thread;
    };
} // namespace djw::core::library

#endif // DJW_CORE_LIBRARY_LIBRARY_SERVICE_HPP
