// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library/playlist_store.hpp"

#include "library/track_store.hpp"

#include <utility>

namespace djw::core::library
{
    std::expected<PlaylistStore, std::string> PlaylistStore::open(SqliteDatabase &database)
    {
        auto list   = database.prepare(R"sql(
            SELECT p.id, p.name, count(e.track_id) FROM playlists p
            LEFT JOIN playlist_entries e ON e.playlist_id = p.id
            GROUP BY p.id ORDER BY p.name COLLATE NOCASE
        )sql");
        auto tracks = database.prepare("SELECT " + std::string{trackColumns()} +
                                       " FROM playlist_entries e JOIN tracks t ON t.id = e.track_id"
                                       " WHERE e.playlist_id = ? ORDER BY e.position");
        auto entries =
            database.prepare("SELECT track_id FROM playlist_entries WHERE playlist_id = ? ORDER BY position");
        auto id_of  = database.prepare("SELECT id FROM playlists WHERE name = ?");
        auto create = database.prepare("INSERT INTO playlists (name) VALUES (?) RETURNING id");
        auto rename = database.prepare("UPDATE playlists SET name = ? WHERE id = ?");
        auto remove = database.prepare("DELETE FROM playlists WHERE id = ?");
        auto clear  = database.prepare("DELETE FROM playlist_entries WHERE playlist_id = ?");
        auto insert =
            database.prepare("INSERT INTO playlist_entries (playlist_id, position, track_id) VALUES (?, ?, ?)");
        for (const auto *statement : {&list, &tracks, &entries, &id_of, &create, &rename, &remove, &clear, &insert})
        {
            if (!*statement)
            {
                return std::unexpected{statement->error()};
            }
        }
        return PlaylistStore{database, Statements{
                                           .list    = std::move(*list),
                                           .tracks  = std::move(*tracks),
                                           .entries = std::move(*entries),
                                           .id_of   = std::move(*id_of),
                                           .create  = std::move(*create),
                                           .rename  = std::move(*rename),
                                           .remove  = std::move(*remove),
                                           .clear   = std::move(*clear),
                                           .insert  = std::move(*insert),
                                       }};
    }

    std::expected<std::vector<api::Playlist>, std::string> PlaylistStore::list()
    {
        return collectRows(statements.list,
                           [](const Statement &row)
                           {
                               return api::Playlist{.id          = row.integer(0),
                                                    .name        = row.text(1),
                                                    .track_count = static_cast<std::size_t>(row.integer(2))};
                           });
    }

    std::expected<std::vector<api::LibraryTrack>, std::string> PlaylistStore::tracks(std::int64_t playlist_id)
    {
        return collectRows(statements.tracks.bind(1, playlist_id), readTrack);
    }

    std::expected<std::vector<std::int64_t>, std::string> PlaylistStore::entries(std::int64_t playlist_id)
    {
        return collectRows(statements.entries.bind(1, playlist_id),
                           [](const Statement &row) { return row.integer(0); });
    }

    std::expected<std::optional<std::int64_t>, std::string> PlaylistStore::idOf(std::string_view name)
    {
        auto ids = collectRows(statements.id_of.bind(1, name), [](const Statement &row) { return row.integer(0); });
        if (!ids)
        {
            return std::unexpected{ids.error()};
        }
        return ids->empty() ? std::nullopt : std::optional{ids->front()};
    }

    std::expected<std::int64_t, std::string> PlaylistStore::create(std::string_view name)
    {
        auto ids = collectRows(statements.create.bind(1, name), [](const Statement &row) { return row.integer(0); });
        if (!ids)
        {
            return std::unexpected{ids.error()};
        }
        if (ids->empty())
        {
            return std::unexpected{std::string{"creating the playlist returned no id"}};
        }
        return ids->front();
    }

    std::expected<void, std::string> PlaylistStore::rename(std::int64_t playlist_id, std::string_view name)
    {
        return statements.rename.bind(1, name).bind(2, playlist_id).run();
    }

    std::expected<void, std::string> PlaylistStore::remove(std::int64_t playlist_id)
    {
        return statements.remove.bind(1, playlist_id).run();
    }

    std::expected<void, std::string> PlaylistStore::setEntries(std::int64_t                     playlist_id,
                                                               const std::vector<std::int64_t> &track_ids)
    {
        if (auto begun = database->execute("SAVEPOINT set_entries"); !begun)
        {
            return begun;
        }
        auto written = statements.clear.bind(1, playlist_id).run();
        for (std::size_t position = 0; written && position < track_ids.size(); ++position)
        {
            written = statements.insert.bind(1, playlist_id)
                          .bind(2, static_cast<std::int64_t>(position))
                          .bind(3, track_ids[position])
                          .run();
        }
        if (!written)
        {
            if (const auto rolled_back = database->execute("ROLLBACK TO set_entries; RELEASE set_entries");
                !rolled_back)
            {
                return std::unexpected{written.error() + "; " + rolled_back.error()};
            }
            return written;
        }
        return database->execute("RELEASE set_entries");
    }
} // namespace djw::core::library
