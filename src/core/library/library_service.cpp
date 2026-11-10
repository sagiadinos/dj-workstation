// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library/library_service.hpp"

#include "library/missing_files.hpp"
#include "library/utf8_path.hpp"

#include <system_error>
#include <utility>

namespace djw::core::library
{
    std::expected<std::unique_ptr<LibraryService>, std::string>
    LibraryService::open(const std::filesystem::path &database_file, TagReader read_tags, TagWriter write_tags)
    {
        auto reader = TrackStore::open(database_file);
        if (!reader)
        {
            return std::unexpected{reader.error()};
        }
        auto writer = TrackStore::open(database_file);
        if (!writer)
        {
            return std::unexpected{writer.error()};
        }
        return std::unique_ptr<LibraryService>{
            new LibraryService{std::move(*reader), std::move(*writer), std::move(read_tags), std::move(write_tags)}};
    }

    LibraryService::LibraryService(TrackStore reader, TrackStore writer, TagReader read_tags, TagWriter write_tags)
        : reader{std::move(reader)}, writer{std::move(writer)}, read_tags{std::move(read_tags)},
          write_tags{std::move(write_tags)}
    {
        tracks_total  = this->reader.trackCount().value_or(0);
        missing_total = this->reader.missingCount().value_or(0);
        thread        = std::thread{[this] { run(); }};
    }

    LibraryService::~LibraryService()
    {
        stopping.store(true);
        {
            const std::scoped_lock lock{mutex};
            scan_requested = false;
            edits.clear();
        }
        wake.notify_all();
        thread.join();
    }

    std::expected<void, std::string> LibraryService::addFolder(const std::filesystem::path &folder)
    {
        std::error_code error;
        const auto      absolute = std::filesystem::canonical(folder, error);
        if (error || !std::filesystem::is_directory(absolute, error))
        {
            return std::unexpected{"not a folder: " + toUtf8(folder)};
        }
        if (auto added = reader.addFolder(absolute); !added)
        {
            return added;
        }
        rescan();
        return {};
    }

    void LibraryService::rescan()
    {
        {
            const std::scoped_lock lock{mutex};
            scan_requested = true;
        }
        wake.notify_all();
    }

    void LibraryService::updateTags(std::int64_t track_id, api::LibraryTagChange change)
    {
        {
            const std::scoped_lock lock{mutex};
            edits.push_back({.track_id = track_id, .change = std::move(change)});
        }
        wake.notify_all();
    }

    void LibraryService::removeMissing()
    {
        {
            const std::scoped_lock lock{mutex};
            remove_requested = true;
        }
        wake.notify_all();
    }

    std::vector<std::filesystem::path> LibraryService::folders() const
    {
        return reader.folders().value_or(std::vector<std::filesystem::path>{});
    }

    api::LibraryStatus LibraryService::status() const
    {
        const std::scoped_lock lock{mutex};
        return {
            .scanning      = scanning || scan_requested,
            .files_found   = counters.files_found.load(std::memory_order_relaxed),
            .files_updated = counters.files_updated.load(std::memory_order_relaxed),
            .tracks_total  = tracks_total,
            .missing_total = missing_total,
            .pending_edits = edits.size() + edits_writing,
            .revision      = revision,
            .scan_error    = scan_error,
            .edit_error    = edit_error,
        };
    }

    std::expected<std::vector<api::LibraryTrack>, std::string>
    LibraryService::search(const api::LibraryQuery &query) const
    {
        return reader.search(query);
    }

    void LibraryService::run() // library thread
    {
        while (true)
        {
            bool scan_now = false;
            {
                std::unique_lock lock{mutex};
                wake.wait(lock,
                          [this] { return scan_requested || remove_requested || !edits.empty() || stopping.load(); });
                if (stopping.load())
                {
                    return;
                }
                scan_now       = scan_requested;
                scan_requested = false;
                if (scan_now)
                {
                    scanning = true;
                    scan_error.clear();
                }
            }
            writePendingEdits();
            removeMissingTracks();
            if (scan_now)
            {
                scan();
            }
        }
    }

    void LibraryService::scan() // library thread
    {
        counters.files_found.store(0);
        counters.files_updated.store(0);

        std::string error;
        if (auto folders = writer.folders(); !folders)
        {
            error = folders.error();
        }
        else
        {
            error = scanFolders(*folders, writer, read_tags, counters, stopping, [this] { writeEditsDuringScan(); });
        }
        if (std::string missing_error = checkMissingFiles(writer, stopping); error.empty())
        {
            error = std::move(missing_error);
        }
        updateCounts();

        const std::scoped_lock lock{mutex};
        scanning   = false;
        scan_error = std::move(error);
        ++revision;
    }

    void LibraryService::updateCounts() // library thread
    {
        const auto tracks  = writer.trackCount();
        const auto missing = writer.missingCount();

        const std::scoped_lock lock{mutex};
        tracks_total  = tracks.value_or(tracks_total);
        missing_total = missing.value_or(missing_total);
    }

    void LibraryService::removeMissingTracks() // library thread
    {
        {
            const std::scoped_lock lock{mutex};
            if (!remove_requested)
            {
                return;
            }
            remove_requested = false;
        }
        const auto removed = writer.removeMissing();
        updateCounts();

        const std::scoped_lock lock{mutex};
        if (removed)
        {
            ++revision;
        }
        else
        {
            edit_error = removed.error();
        }
    }

    void LibraryService::writeEditsDuringScan() // library thread
    {
        {
            const std::scoped_lock lock{mutex};
            if (edits.empty())
            {
                return;
            }
        }
        // Committed first, so the edit is visible to searches as soon as the revision says so
        auto committed = writer.commitTransaction();
        writePendingEdits();
        if (committed)
        {
            committed = writer.beginTransaction();
        }
        if (!committed)
        {
            const std::scoped_lock lock{mutex};
            edit_error = committed.error();
        }
    }

    void LibraryService::writePendingEdits() // library thread
    {
        while (!stopping.load())
        {
            TagEdit edit;
            {
                const std::scoped_lock lock{mutex};
                if (edits.empty())
                {
                    return;
                }
                edit = std::move(edits.front());
                edits.pop_front();
                edits_writing = 1;
            }
            const auto written = write(edit);

            const std::scoped_lock lock{mutex};
            edits_writing = 0;
            if (written)
            {
                edit_error.clear();
                ++revision;
            }
            else
            {
                edit_error = written.error();
            }
        }
    }

    std::expected<void, std::string> LibraryService::write(const TagEdit &edit) // library thread
    {
        const auto path = writer.trackPath(edit.track_id);
        if (!path)
        {
            return std::unexpected{path.error()};
        }
        if (!*path)
        {
            return std::unexpected{"cannot write tags: track " + std::to_string(edit.track_id) +
                                   " is no longer in the library"};
        }
        if (auto written = write_tags(**path, edit.change); !written)
        {
            return written;
        }
        // Read back, so the database holds what is in the file now, with the new stamp of the changed file
        const auto stamp = fileStamp(**path);
        if (!stamp)
        {
            return std::unexpected{stamp.error()};
        }
        return writer.store(**path, *stamp, read_tags(**path));
    }
} // namespace djw::core::library
