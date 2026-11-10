// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_LIBRARY_TAG_WRITER_HPP
#define DJW_CORE_LIBRARY_TAG_WRITER_HPP

#include "api/library.hpp"

#include <expected>
#include <filesystem>
#include <functional>
#include <string>

namespace djw::core::library
{
    /// Writes a tag change into a file; injected into the library, so tests can fake failures
    using TagWriter = std::function<std::expected<void, std::string>(const std::filesystem::path &file,
                                                                     const api::LibraryTagChange &)>;

    /**
     * Writes the changed fields with TagLib in the file's own tag format (ID3v2 for MP3, Vorbis comments for Ogg and
     * FLAC, RIFF INFO/ID3v2 for WAV) and saves the file; fields without a value stay as they are. A file without
     * audio TagLib can read is left alone. An error names the file.
     */
    std::expected<void, std::string> writeTags(const std::filesystem::path &file, const api::LibraryTagChange &change);
} // namespace djw::core::library

#endif // DJW_CORE_LIBRARY_TAG_WRITER_HPP
