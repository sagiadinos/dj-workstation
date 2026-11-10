// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_LIBRARY_FILE_NAME_TAGS_HPP
#define DJW_CORE_LIBRARY_FILE_NAME_TAGS_HPP

#include "library/track_tags.hpp"

#include <filesystem>

namespace djw::core::library
{
    /**
     * Artist and title from the file name, for files without tags: "Artist - Title.mp3" gives both, split at the
     * first " - "; any other name becomes the title. Underscores count as spaces.
     */
    TrackTags tagsFromFileName(const std::filesystem::path &file);

    /// Fills empty artist and title of tags from the file name; everything else stays as it is
    TrackTags withFileNameFallback(TrackTags tags, const std::filesystem::path &file);
} // namespace djw::core::library

#endif // DJW_CORE_LIBRARY_FILE_NAME_TAGS_HPP
