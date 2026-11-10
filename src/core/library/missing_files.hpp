// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_LIBRARY_MISSING_FILES_HPP
#define DJW_CORE_LIBRARY_MISSING_FILES_HPP

#include "library/track_store.hpp"

#include <atomic>
#include <string>

namespace djw::core::library
{
    /**
     * Dead link check: marks every track whose file does not exist as missing and unmarks those whose file is there
     * (again). Only changed marks are written, all in one transaction. Stops between two tracks when stop is set.
     * Returns the error that stopped the check, empty if none.
     */
    std::string checkMissingFiles(TrackStore &store, const std::atomic<bool> &stop);
} // namespace djw::core::library

#endif // DJW_CORE_LIBRARY_MISSING_FILES_HPP
