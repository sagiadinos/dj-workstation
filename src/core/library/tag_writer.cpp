// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library/tag_writer.hpp"

#include "library/utf8_path.hpp"

#include <algorithm>

// Without the taglib/ prefix: the source tree of TagLib has no such directory
#include <fileref.h>
#include <tag.h>
#include <tstring.h>

namespace djw::core::library
{
    namespace
    {
        TagLib::String fromUtf8(const std::string &text)
        {
            return {text, TagLib::String::UTF8};
        }
    } // namespace

    std::expected<void, std::string> writeTags(const std::filesystem::path &file, const api::LibraryTagChange &change)
    {
        // With audio properties: TagLib also opens a file without a single audio frame and would tag it
        TagLib::FileRef reference{file.c_str(), true, TagLib::AudioProperties::Fast};
        const bool      is_audio = !reference.isNull() && reference.tag() != nullptr &&
                                   reference.audioProperties() != nullptr &&
                                   reference.audioProperties()->lengthInMilliseconds() > 0;
        if (!is_audio)
        {
            return std::unexpected{"cannot write tags of " + toUtf8(file) + ": not a readable audio file"};
        }
        TagLib::Tag *tag = reference.tag();
        if (change.artist)
        {
            // mull-ignore-next: cxx_remove_void_call
            tag->setArtist(fromUtf8(*change.artist));
        }
        if (change.title)
        {
            // mull-ignore-next: cxx_remove_void_call
            tag->setTitle(fromUtf8(*change.title));
        }
        if (change.album)
        {
            // mull-ignore-next: cxx_remove_void_call
            tag->setAlbum(fromUtf8(*change.album));
        }
        if (change.genre)
        {
            // mull-ignore-next: cxx_remove_void_call
            tag->setGenre(fromUtf8(*change.genre));
        }
        if (change.year)
        {
            tag->setYear(static_cast<unsigned int>(std::max(0, *change.year)));
        }
        if (!reference.save())
        {
            return std::unexpected{"cannot write tags of " + toUtf8(file) + ": the file cannot be saved"};
        }
        return {};
    }
} // namespace djw::core::library
