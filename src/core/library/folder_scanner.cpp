// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library/folder_scanner.hpp"

#include "library/utf8_path.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>
#include <system_error>

namespace djw::core::library
{
    namespace
    {
        // Files per transaction: small enough for the search to show progress, large enough to be fast
        constexpr std::size_t batch_files = 100;

        constexpr std::array<std::string_view, 4> audio_extensions{".mp3", ".ogg", ".flac", ".wav"};

        /// Keeps the first error and goes on
        class ErrorLog
        {
            public:
                void add(std::string error)
                {
                    if (first.empty())
                    {
                        first = std::move(error);
                    }
                }

                [[nodiscard]] std::string take() { return std::move(first); }

            private:
                std::string first;
        };

        /// Stores one file if it is new or changed
        std::expected<void, std::string> scanFile(const std::filesystem::path &file, TrackStore &store,
                                                  const TagReader &read_tags, ScanCounters &counters)
        {
            counters.files_found.fetch_add(1, std::memory_order_relaxed);
            const auto current = fileStamp(file);
            if (!current)
            {
                return std::unexpected{current.error()};
            }
            const auto stored = store.stamp(file);
            if (!stored)
            {
                return std::unexpected{stored.error()};
            }
            if (*stored == *current)
            {
                return {};
            }
            if (auto written = store.store(file, *current, read_tags(file)); !written)
            {
                return written;
            }
            counters.files_updated.fetch_add(1, std::memory_order_relaxed);
            return {};
        }

        /// Commits every batch_files files, so the reader connection sees the progress
        class Batch
        {
            public:
                explicit Batch(TrackStore &store) : store{&store} {}

                std::expected<void, std::string> next()
                {
                    if (open && ++files < batch_files)
                    {
                        return {};
                    }
                    if (auto committed = finish(); !committed)
                    {
                        return committed;
                    }
                    files = 0;
                    open  = true;
                    return store->beginTransaction();
                }

                std::expected<void, std::string> finish()
                {
                    if (!open)
                    {
                        return {};
                    }
                    open = false;
                    return store->commitTransaction();
                }

            private:
                TrackStore *store;
                std::size_t files = 0;
                bool        open  = false;
        };
    } // namespace

    std::expected<FileStamp, std::string> fileStamp(const std::filesystem::path &file)
    {
        std::error_code error;
        const auto      modified = std::filesystem::last_write_time(file, error);
        const auto      size     = error ? 0 : std::filesystem::file_size(file, error);
        if (error)
        {
            return std::unexpected{"cannot read " + toUtf8(file) + ": " + error.message()};
        }
        return FileStamp{.modified = static_cast<std::int64_t>(modified.time_since_epoch().count()), .size = size};
    }

    bool isAudioFile(const std::filesystem::path &file)
    {
        std::string extension = toUtf8(file.extension());
        std::ranges::transform(extension, extension.begin(),
                               [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        return std::ranges::find(audio_extensions, extension) != audio_extensions.end();
    }

    std::string scanFolders(const std::vector<std::filesystem::path> &folders, TrackStore &store,
                            const TagReader &read_tags, ScanCounters &counters, const std::atomic<bool> &stop,
                            const BetweenFiles &between_files)
    {
        ErrorLog errors;
        Batch    batch{store};
        for (const auto &folder : folders)
        {
            if (stop.load(std::memory_order_relaxed))
            {
                break;
            }
            std::error_code error;
            auto            entry = std::filesystem::recursive_directory_iterator{
                folder, std::filesystem::directory_options::skip_permission_denied, error};
            if (error)
            {
                // mull-ignore-next: cxx_remove_void_call
                errors.add("cannot read folder " + toUtf8(folder) + ": " + error.message());
                continue;
            }
            for (; entry != std::filesystem::recursive_directory_iterator{}; entry.increment(error))
            {
                if (stop.load(std::memory_order_relaxed))
                {
                    break;
                }
                if (error)
                {
                    errors.add("cannot read folder " + toUtf8(folder) + ": " + error.message());
                    break;
                }
                if (!entry->is_regular_file(error) || !isAudioFile(entry->path()))
                {
                    continue;
                }
                if (auto batched = batch.next(); !batched)
                {
                    errors.add(batched.error());
                    return errors.take(); // the database itself fails, the other files would fail too
                }
                if (auto scanned = scanFile(entry->path(), store, read_tags, counters); !scanned)
                {
                    errors.add(scanned.error());
                }
                if (between_files)
                {
                    between_files();
                }
            }
        }
        if (auto committed = batch.finish(); !committed)
        {
            errors.add(committed.error());
        }
        return errors.take();
    }
} // namespace djw::core::library
