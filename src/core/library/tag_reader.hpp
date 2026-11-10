// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_LIBRARY_TAG_READER_HPP
#define DJW_CORE_LIBRARY_TAG_READER_HPP

#include "library/track_tags.hpp"

#include <filesystem>

namespace djw::core::library
{
    /**
     * Reads tags and duration with TagLib (ID3v2, Vorbis comments, FLAC, RIFF INFO, ...). Missing artist or title
     * come from the file name, see withFileNameFallback; a file TagLib cannot read gets only those.
     */
    TrackTags readTags(const std::filesystem::path &file);
} // namespace djw::core::library

#endif // DJW_CORE_LIBRARY_TAG_READER_HPP
