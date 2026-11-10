// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library/playlist_files.hpp"

#include "library/folder_scanner.hpp"
#include "library/m3u.hpp"
#include "library/utf8_path.hpp"

#include <fstream>
#include <iterator>
#include <optional>
#include <system_error>
#include <vector>

namespace djw::core::library
{
    namespace
    {
        std::expected<std::string, std::string> readFile(const std::filesystem::path &file)
        {
            std::ifstream in{file, std::ios::binary};
            if (!in)
            {
                return std::unexpected{"cannot read " + toUtf8(file)};
            }
            return std::string{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
        }

        /// The file's track, added to the library first if it is an existing audio file; std::nullopt if not found
        std::expected<std::optional<std::int64_t>, std::string> trackFor(const std::filesystem::path &file,
                                                                         TrackStore &tracks, const TagReader &read_tags)
        {
            auto known = tracks.trackId(file);
            if (!known || *known)
            {
                return known;
            }
            std::error_code error;
            if (!std::filesystem::is_regular_file(file, error) || !isAudioFile(file))
            {
                return std::nullopt;
            }
            const auto stamp = fileStamp(file);
            if (!stamp)
            {
                return std::nullopt;
            }
            if (auto stored = tracks.store(file, *stamp, read_tags(file)); !stored)
            {
                return std::unexpected{stored.error()};
            }
            return tracks.trackId(file);
        }

        /// name, or name (2), name (3), ... whichever is free
        std::expected<std::string, std::string> freeName(const std::string &name, PlaylistStore &playlists)
        {
            std::string candidate = name;
            for (int number = 2;; ++number)
            {
                const auto taken = playlists.idOf(candidate);
                if (!taken)
                {
                    return std::unexpected{taken.error()};
                }
                if (!*taken)
                {
                    return candidate;
                }
                candidate = name + " (" + std::to_string(number) + ")";
            }
        }

        std::expected<api::PlaylistImport, std::string> importEntries(const std::filesystem::path &file,
                                                                      TrackStore &tracks, PlaylistStore &playlists,
                                                                      const TagReader &read_tags)
        {
            const auto content = readFile(file);
            if (!content)
            {
                return std::unexpected{content.error()};
            }
            api::PlaylistImport       result;
            std::vector<std::int64_t> track_ids;
            for (const std::filesystem::path &entry : parseM3u(*content, file.parent_path()))
            {
                const auto track = trackFor(entry, tracks, read_tags);
                if (!track)
                {
                    return std::unexpected{track.error()};
                }
                if (*track)
                {
                    track_ids.push_back(**track);
                }
                else
                {
                    ++result.not_found;
                }
            }
            const auto name = freeName(toUtf8(file.stem()), playlists);
            if (!name)
            {
                return std::unexpected{name.error()};
            }
            const auto playlist = playlists.create(*name);
            if (!playlist)
            {
                return std::unexpected{playlist.error()};
            }
            if (auto written = playlists.setEntries(*playlist, track_ids); !written)
            {
                return std::unexpected{written.error()};
            }
            result.playlist = *name;
            result.tracks   = track_ids.size();
            return result;
        }
    } // namespace

    std::expected<api::PlaylistImport, std::string> importM3uFile(const std::filesystem::path &file, TrackStore &tracks,
                                                                  PlaylistStore &playlists, const TagReader &read_tags)
    {
        // One transaction: new tracks, playlist and entries come together or not at all
        if (auto begun = tracks.beginTransaction(); !begun)
        {
            return std::unexpected{begun.error()};
        }
        auto imported = importEntries(file, tracks, playlists, read_tags);
        if (!imported)
        {
            if (auto rolled_back = tracks.connection().execute("ROLLBACK"); !rolled_back)
            {
                return std::unexpected{imported.error() + "; " + rolled_back.error()};
            }
            return imported;
        }
        if (auto committed = tracks.commitTransaction(); !committed)
        {
            return std::unexpected{committed.error()};
        }
        return imported;
    }

    std::expected<void, std::string> exportM3uFile(std::int64_t playlist_id, const std::filesystem::path &file,
                                                   PlaylistStore &playlists)
    {
        const auto tracks = playlists.tracks(playlist_id);
        if (!tracks)
        {
            return std::unexpected{tracks.error()};
        }
        const std::string text = formatM3u(*tracks);
        std::ofstream     out{file, std::ios::binary | std::ios::trunc};
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        out.close();
        if (!out)
        {
            return std::unexpected{"cannot write " + toUtf8(file)};
        }
        return {};
    }
} // namespace djw::core::library
