// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_TESTS_PLAYER_UI_SDL_FAKE_LIBRARY_HPP
#define DJW_TESTS_PLAYER_UI_SDL_FAKE_LIBRARY_HPP

#include "api/library.hpp"
#include "api/playlists.hpp"

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

namespace djw::tests
{
    /**
     * Library and playlists in memory for the view tests. Search returns the tracks whose title contains the text.
     * Writes happen at once and raise the revision, as if the library thread had done them; what was asked for is
     * recorded.
     */
    class FakeLibrary final : public core::api::Library, public core::api::Playlists
    {
        public:
            // api::Library
            std::expected<void, std::string> addFolder(const std::filesystem::path &folder) override
            {
                added_folders.push_back(folder);
                if (!add_folder_error.empty())
                {
                    return std::unexpected{add_folder_error};
                }
                return {};
            }
            void                               rescan() override { ++rescans; }
            std::vector<std::filesystem::path> folders() const override { return {}; }
            core::api::LibraryStatus           status() const override { return library_status; }
            core::api::Playlists              &playlists() noexcept override { return *this; }

            void removeMissing() override { ++removals; }

            void updateTags(std::int64_t track_id, core::api::LibraryTagChange change) override
            {
                edits.emplace_back(track_id, std::move(change));
            }

            std::expected<std::vector<core::api::LibraryTrack>, std::string>
            search(const core::api::LibraryQuery &query) const override
            {
                ++searches;
                last_query = query;
                if (fail)
                {
                    return std::unexpected{std::string{"database broken"}};
                }
                std::vector<core::api::LibraryTrack> found;
                for (const auto &track : library_tracks)
                {
                    if (track.title.find(query.text) != std::string::npos)
                    {
                        found.push_back(track);
                    }
                }
                return found;
            }

            // api::Playlists
            std::expected<std::vector<core::api::Playlist>, std::string> list() const override
            {
                std::vector<core::api::Playlist> result;
                for (const auto &[id, entry] : stored)
                {
                    result.push_back({.id = id, .name = entry.name, .track_count = entry.tracks.size()});
                }
                std::ranges::sort(result, {}, &core::api::Playlist::name);
                return result;
            }

            std::expected<std::vector<core::api::LibraryTrack>, std::string> tracks(std::int64_t id) const override
            {
                std::vector<core::api::LibraryTrack> result;
                if (const auto found = stored.find(id); found != stored.end())
                {
                    for (const std::int64_t track_id : found->second.tracks)
                    {
                        result.push_back(*std::ranges::find(library_tracks, track_id, &core::api::LibraryTrack::id));
                    }
                }
                return result;
            }

            void create(std::string name) override
            {
                if (std::ranges::any_of(stored, [&name](const auto &entry) { return entry.second.name == name; }))
                {
                    library_status.write_error = "a playlist named " + name + " exists already";
                    return;
                }
                stored[next_id++] = {.name = std::move(name), .tracks = {}};
                changed();
            }

            void rename(std::int64_t id, std::string name) override
            {
                renames.emplace_back(id, name);
                stored.at(id).name = std::move(name);
                changed();
            }

            void remove(std::int64_t id) override
            {
                stored.erase(id);
                changed();
            }

            void add(std::int64_t id, std::vector<std::int64_t> track_ids) override
            {
                auto &tracks = stored.at(id).tracks;
                tracks.insert(tracks.end(), track_ids.begin(), track_ids.end());
                changed();
            }

            void removeEntry(std::int64_t id, std::size_t index) override
            {
                auto &tracks = stored.at(id).tracks;
                tracks.erase(tracks.begin() + static_cast<std::ptrdiff_t>(index));
                changed();
            }

            void moveEntry(std::int64_t id, std::size_t from, std::size_t to) override
            {
                auto      &tracks = stored.at(id).tracks;
                const auto moved  = tracks[from];
                tracks.erase(tracks.begin() + static_cast<std::ptrdiff_t>(from));
                tracks.insert(tracks.begin() + static_cast<std::ptrdiff_t>(to), moved);
                changed();
            }

            void importM3u(std::filesystem::path file) override
            {
                const std::string name     = file.stem().string();
                stored[next_id++]          = {.name = name, .tracks = {1}};
                library_status.last_import = core::api::PlaylistImport{.playlist = name, .tracks = 1, .not_found = 2};
                imports.push_back(std::move(file));
                changed();
            }

            void exportM3u(std::int64_t id, std::filesystem::path file) override
            {
                exports.emplace_back(id, std::move(file));
            }

            /// Creates a playlist with tracks directly, without a revision change
            std::int64_t addPlaylist(std::string name, std::vector<std::int64_t> tracks)
            {
                stored[next_id] = {.name = std::move(name), .tracks = std::move(tracks)};
                return next_id++;
            }

            std::vector<core::api::LibraryTrack> library_tracks{
                {.id = 1, .path = "/m/a.mp3", .artist = "A", .title = "alpha"},
                {.id = 2, .path = "/m/b.mp3", .artist = "B", .title = "beta"},
                {.id = 3, .path = "/m/c.mp3", .artist = "C", .title = "gamma"},
            };
            std::vector<std::filesystem::path>                                added_folders;
            std::string                                                       add_folder_error;
            int                                                               rescans = 0;
            core::api::LibraryStatus                                          library_status;
            std::vector<std::pair<std::int64_t, core::api::LibraryTagChange>> edits;
            int                                                               removals = 0;
            bool                                                              fail     = false;
            mutable int                                                       searches = 0;
            mutable core::api::LibraryQuery                                   last_query;
            std::vector<std::pair<std::int64_t, std::string>>                 renames;
            std::vector<std::filesystem::path>                                imports;
            std::vector<std::pair<std::int64_t, std::filesystem::path>>       exports;

        private:
            struct Stored
            {
                    std::string               name;
                    std::vector<std::int64_t> tracks;
            };

            void changed() { ++library_status.revision; }

            std::map<std::int64_t, Stored> stored;
            std::int64_t                   next_id = 1;
    };
} // namespace djw::tests

#endif // DJW_TESTS_PLAYER_UI_SDL_FAKE_LIBRARY_HPP
