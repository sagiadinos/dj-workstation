// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library/missing_files.hpp"

#include <filesystem>
#include <system_error>

namespace djw::core::library
{
    std::string checkMissingFiles(TrackStore &store, const std::atomic<bool> &stop)
    {
        const auto files = store.files();
        if (!files)
        {
            return files.error();
        }
        if (auto begun = store.beginTransaction(); !begun)
        {
            return begun.error();
        }
        std::string error;
        for (const StoredFile &file : *files)
        {
            if (stop.load(std::memory_order_relaxed))
            {
                break;
            }
            // An error (e.g. no permission for a folder on the way) counts as missing: the track cannot be played
            std::error_code ignored;
            const bool      missing = !std::filesystem::is_regular_file(file.path, ignored);
            if (missing == file.missing)
            {
                continue;
            }
            if (auto marked = store.setMissing(file.track_id, missing); !marked)
            {
                error = marked.error();
                break;
            }
        }
        if (auto committed = store.commitTransaction(); !committed && error.empty())
        {
            error = committed.error();
        }
        return error;
    }
} // namespace djw::core::library
