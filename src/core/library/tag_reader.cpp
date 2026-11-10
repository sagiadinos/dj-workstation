// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library/tag_reader.hpp"

#include "library/file_name_tags.hpp"

// Without the taglib/ prefix: the source tree of TagLib has no such directory
#include <fileref.h>
#include <tag.h>
#include <tdebuglistener.h>
#include <tstring.h>

namespace djw::core::library
{
    namespace
    {
        std::string utf8(const TagLib::String &text)
        {
            return text.to8Bit(true);
        }

        /// TagLib reports every unreadable file on stderr; for a library scan that is noise, the file is just skipped
        class SilentDebugListener final : public TagLib::DebugListener
        {
            public:
                void printMessage(const TagLib::String &) override {}
        };

        void silenceTagLib()
        {
            static SilentDebugListener listener;
            static const bool          installed = []
            {
                TagLib::setDebugListener(&listener);
                return true;
            }();
            static_cast<void>(installed);
        }
    } // namespace

    TrackTags readTags(const std::filesystem::path &file)
    {
        silenceTagLib();
        TrackTags tags;
        // The path type matches TagLib's file name type on each platform
        const TagLib::FileRef reference{file.c_str(), true, TagLib::AudioProperties::Fast};
        if (!reference.isNull())
        {
            if (const TagLib::Tag *tag = reference.tag())
            {
                tags.artist = utf8(tag->artist());
                tags.title  = utf8(tag->title());
                tags.album  = utf8(tag->album());
                tags.genre  = utf8(tag->genre());
                tags.year   = static_cast<int>(tag->year());
            }
            if (const TagLib::AudioProperties *properties = reference.audioProperties())
            {
                tags.duration_seconds = properties->lengthInMilliseconds() / 1'000.0;
            }
        }
        return withFileNameFallback(std::move(tags), file);
    }
} // namespace djw::core::library
