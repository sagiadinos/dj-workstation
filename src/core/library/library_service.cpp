// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library/library_service.hpp"

#include "library/utf8_path.hpp"

#include <system_error>
#include <utility>

namespace djw::core::library
{
    std::expected<std::unique_ptr<LibraryService>, std::string>
    LibraryService::open(const std::filesystem::path &database_file, TagReader read_tags)
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
            new LibraryService{std::move(*reader), std::move(*writer), std::move(read_tags)}};
    }

    LibraryService::LibraryService(TrackStore reader, TrackStore writer, TagReader read_tags)
        : reader{std::move(reader)}, writer{std::move(writer)}, read_tags{std::move(read_tags)}
    {
        const auto count = this->reader.trackCount();
        tracks_total     = count.value_or(0);
        thread           = std::thread{[this] { run(); }};
    }

    LibraryService::~LibraryService()
    {
        stopping.store(true);
        {
            const std::scoped_lock lock{mutex};
            scan_requested = false;
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

    std::vector<std::filesystem::path> LibraryService::folders() const
    {
        return reader.folders().value_or(std::vector<std::filesystem::path>{});
    }

    api::LibraryScanStatus LibraryService::scanStatus() const
    {
        const std::scoped_lock lock{mutex};
        return {
            .running       = scanning || scan_requested,
            .files_found   = counters.files_found.load(std::memory_order_relaxed),
            .files_updated = counters.files_updated.load(std::memory_order_relaxed),
            .tracks_total  = tracks_total,
            .error         = scan_error,
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
            {
                std::unique_lock lock{mutex};
                wake.wait(lock, [this] { return scan_requested || stopping.load(); });
                if (stopping.load())
                {
                    return;
                }
                scan_requested = false;
                scanning       = true;
                scan_error.clear();
            }
            scan();
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
            error = scanFolders(*folders, writer, read_tags, counters, stopping);
        }
        const auto count = writer.trackCount();

        const std::scoped_lock lock{mutex};
        scanning     = false;
        scan_error   = std::move(error);
        tracks_total = count.value_or(tracks_total);
    }
} // namespace djw::core::library
