// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_LIBRARY_FOLDER_SCANNER_HPP
#define DJW_CORE_LIBRARY_FOLDER_SCANNER_HPP

#include "library/track_store.hpp"
#include "library/track_tags.hpp"

#include <atomic>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace djw::core::library
{
    /// Progress of a scan, readable from other threads while it runs
    struct ScanCounters
    {
            std::atomic<std::size_t> files_found{0};
            std::atomic<std::size_t> files_updated{0};
    };

    /// MP3, Ogg Vorbis, FLAC and WAV, by extension in any case
    bool isAudioFile(const std::filesystem::path &file);

    /// Modification time and size of a file as stored with its track
    std::expected<FileStamp, std::string> fileStamp(const std::filesystem::path &file);

    /// Called between two files of a scan, inside the scan's transaction; the library writes tag edits there
    using BetweenFiles = std::function<void()>;

    /**
     * Walks the folders with their subfolders and stores every audio file in store. Tags are read only for new
     * files and files whose modification time or size changed. Committed in batches, so a search sees the progress.
     *
     * Stops between two files when stop is set. A folder or file that cannot be read is skipped, the scan goes on;
     * the first such error is returned (empty if none).
     */
    std::string scanFolders(const std::vector<std::filesystem::path> &folders, TrackStore &store,
                            const TagReader &read_tags, ScanCounters &counters, const std::atomic<bool> &stop,
                            const BetweenFiles &between_files = {});
} // namespace djw::core::library

#endif // DJW_CORE_LIBRARY_FOLDER_SCANNER_HPP
