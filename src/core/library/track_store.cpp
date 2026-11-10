// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library/track_store.hpp"

#include "library/search_query.hpp"
#include "library/text_fold.hpp"
#include "library/utf8_path.hpp"

#include <type_traits>
#include <utility>

namespace djw::core::library
{
    namespace
    {
        constexpr std::string_view schema = R"sql(
            CREATE TABLE folders (
                path TEXT PRIMARY KEY
            );
            CREATE TABLE tracks (
                id        INTEGER PRIMARY KEY,
                path      TEXT    NOT NULL UNIQUE,
                file_name TEXT    NOT NULL,
                modified  INTEGER NOT NULL,
                size      INTEGER NOT NULL,
                artist    TEXT    NOT NULL,
                title     TEXT    NOT NULL,
                album     TEXT    NOT NULL,
                genre     TEXT    NOT NULL,
                year      INTEGER NOT NULL,
                duration  REAL    NOT NULL,
                -- Folded (foldText) for filters and sorting: lower case, without accents
                artist_key TEXT NOT NULL,
                title_key  TEXT NOT NULL,
                genre_key  TEXT NOT NULL,
                -- artist, title, album, genre and file name, one per line, for wildcard words of the search text
                any_key    TEXT NOT NULL
            );
            CREATE INDEX tracks_by_artist_title ON tracks (artist_key, title_key);

            -- Full text index over the tracks table; remove_diacritics: "Beyonce" finds "Beyoncé"
            CREATE VIRTUAL TABLE tracks_search USING fts5 (
                artist, title, album, genre, file_name,
                content = 'tracks', content_rowid = 'id', tokenize = 'unicode61 remove_diacritics 2'
            );
            CREATE TRIGGER tracks_insert AFTER INSERT ON tracks BEGIN
                INSERT INTO tracks_search (rowid, artist, title, album, genre, file_name)
                VALUES (new.id, new.artist, new.title, new.album, new.genre, new.file_name);
            END;
            CREATE TRIGGER tracks_delete AFTER DELETE ON tracks BEGIN
                INSERT INTO tracks_search (tracks_search, rowid, artist, title, album, genre, file_name)
                VALUES ('delete', old.id, old.artist, old.title, old.album, old.genre, old.file_name);
            END;
            CREATE TRIGGER tracks_update AFTER UPDATE ON tracks BEGIN
                INSERT INTO tracks_search (tracks_search, rowid, artist, title, album, genre, file_name)
                VALUES ('delete', old.id, old.artist, old.title, old.album, old.genre, old.file_name);
                INSERT INTO tracks_search (rowid, artist, title, album, genre, file_name)
                VALUES (new.id, new.artist, new.title, new.album, new.genre, new.file_name);
            END;
        )sql";

        // Version 1 had no key columns; they are added and filled from the tags
        constexpr std::string_view migration_to_2 = R"sql(
            ALTER TABLE tracks ADD COLUMN artist_key TEXT NOT NULL DEFAULT '';
            ALTER TABLE tracks ADD COLUMN title_key  TEXT NOT NULL DEFAULT '';
            ALTER TABLE tracks ADD COLUMN genre_key  TEXT NOT NULL DEFAULT '';
            ALTER TABLE tracks ADD COLUMN any_key    TEXT NOT NULL DEFAULT '';
            DROP INDEX tracks_by_artist_title;
            CREATE INDEX tracks_by_artist_title ON tracks (artist_key, title_key);
        )sql";

        constexpr std::string_view track_columns =
            "t.id, t.path, t.artist, t.title, t.album, t.genre, t.year, t.duration";

        constexpr std::string_view update_keys =
            "UPDATE tracks SET artist_key = ?, title_key = ?, genre_key = ?, any_key = ? WHERE id = ?";

        struct TrackKeys
        {
                std::string artist;
                std::string title;
                std::string genre;
                std::string any;
        };

        TrackKeys keysOf(const TrackTags &tags, std::string_view file_name)
        {
            return {
                .artist = foldText(tags.artist),
                .title  = foldText(tags.title),
                .genre  = foldText(tags.genre),
                .any    = foldText(tags.artist + '\n' + tags.title + '\n' + tags.album + '\n' + tags.genre + '\n' +
                                   std::string{file_name}),
            };
        }

        api::LibraryTrack readTrack(const Statement &row)
        {
            return {
                .id               = row.integer(0),
                .path             = fromUtf8(row.text(1)),
                .artist           = row.text(2),
                .title            = row.text(3),
                .album            = row.text(4),
                .genre            = row.text(5),
                .year             = static_cast<int>(row.integer(6)),
                .duration_seconds = row.real(7),
            };
        }

        /// Runs query to the end and returns read(row) for every row
        template <typename Read>
        auto collectRows(Statement &query, Read read)
            -> std::expected<std::vector<std::invoke_result_t<Read, const Statement &>>, std::string>
        {
            std::vector<std::invoke_result_t<Read, const Statement &>> rows;
            while (true)
            {
                const auto row = query.step();
                if (!row)
                {
                    query.reset();
                    return std::unexpected{row.error()};
                }
                if (!*row)
                {
                    query.reset();
                    return rows;
                }
                rows.push_back(read(query));
            }
        }

        std::expected<std::int64_t, std::string> userVersion(SqliteDatabase &database)
        {
            auto query = database.prepare("PRAGMA user_version");
            if (!query)
            {
                return std::unexpected{query.error()};
            }
            const auto row = query->step();
            if (!row)
            {
                return std::unexpected{row.error()};
            }
            return *row ? query->integer(0) : 0;
        }
    } // namespace

    std::expected<TrackStore, std::string> TrackStore::open(const std::filesystem::path &database_file)
    {
        auto database = SqliteDatabase::open(database_file);
        if (!database)
        {
            return std::unexpected{database.error()};
        }
        if (auto created = createSchema(*database); !created)
        {
            return std::unexpected{created.error()};
        }
        auto statements = prepareStatements(*database);
        if (!statements)
        {
            return std::unexpected{statements.error()};
        }
        return TrackStore{std::move(*database), std::move(*statements)};
    }

    std::expected<void, std::string> TrackStore::createSchema(SqliteDatabase &database)
    {
        // Exclusive, so two connections opening a new file at the same time do not both create the schema
        if (auto begun = database.execute("BEGIN EXCLUSIVE"); !begun)
        {
            return begun;
        }
        auto                             version = userVersion(database);
        std::expected<void, std::string> result;
        if (!version)
        {
            result = std::unexpected{version.error()};
        }
        else if (*version == 0)
        {
            result = database.execute(std::string{schema} + "PRAGMA user_version = " + std::to_string(schema_version));
        }
        else if (*version == 1)
        {
            result = migrateToVersion2(database);
        }
        else if (*version != schema_version)
        {
            result = std::unexpected{"the library database has schema version " + std::to_string(*version) +
                                     ", this version of djw reads version " + std::to_string(schema_version)};
        }
        if (!result)
        {
            if (const auto rolled_back = database.execute("ROLLBACK"); !rolled_back)
            {
                return std::unexpected{result.error() + "; " + rolled_back.error()};
            }
            return result;
        }
        return database.execute("COMMIT");
    }

    std::expected<void, std::string> TrackStore::migrateToVersion2(SqliteDatabase &database)
    {
        if (auto altered = database.execute(migration_to_2); !altered)
        {
            return altered;
        }
        auto tracks = database.prepare("SELECT id, artist, title, album, genre, file_name FROM tracks");
        auto update = database.prepare(update_keys);
        if (!tracks || !update)
        {
            return std::unexpected{!tracks ? tracks.error() : update.error()};
        }
        while (true)
        {
            const auto row = tracks->step();
            if (!row)
            {
                return std::unexpected{row.error()};
            }
            if (!*row)
            {
                break;
            }
            const TrackKeys keys = keysOf({.artist = tracks->text(1),
                                           .title  = tracks->text(2),
                                           .album  = tracks->text(3),
                                           .genre  = tracks->text(4)},
                                          tracks->text(5));
            if (auto updated = update->bind(1, keys.artist)
                                   .bind(2, keys.title)
                                   .bind(3, keys.genre)
                                   .bind(4, keys.any)
                                   .bind(5, tracks->integer(0))
                                   .run();
                !updated)
            {
                return updated;
            }
        }
        return database.execute("PRAGMA user_version = 2");
    }

    std::expected<TrackStore::Statements, std::string> TrackStore::prepareStatements(SqliteDatabase &database)
    {
        auto add_folder = database.prepare("INSERT OR IGNORE INTO folders (path) VALUES (?)");
        auto folders    = database.prepare("SELECT path FROM folders ORDER BY path");
        auto stamp      = database.prepare("SELECT modified, size FROM tracks WHERE path = ?");
        auto store      = database.prepare(R"sql(
            INSERT INTO tracks (path, file_name, modified, size, artist, title, album, genre, year, duration,
                                artist_key, title_key, genre_key, any_key)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT (path) DO UPDATE SET
                file_name = excluded.file_name, modified = excluded.modified, size = excluded.size,
                artist = excluded.artist, title = excluded.title, album = excluded.album, genre = excluded.genre,
                year = excluded.year, duration = excluded.duration, artist_key = excluded.artist_key,
                title_key = excluded.title_key, genre_key = excluded.genre_key, any_key = excluded.any_key
        )sql");
        auto count      = database.prepare("SELECT count(*) FROM tracks");

        for (const auto *statement : {&add_folder, &folders, &stamp, &store, &count})
        {
            if (!*statement)
            {
                return std::unexpected{statement->error()};
            }
        }
        return Statements{
            .add_folder = std::move(*add_folder),
            .folders    = std::move(*folders),
            .stamp      = std::move(*stamp),
            .store      = std::move(*store),
            .count      = std::move(*count),
        };
    }

    std::expected<void, std::string> TrackStore::addFolder(const std::filesystem::path &folder)
    {
        return statements.add_folder.bind(1, toUtf8(folder)).run();
    }

    std::expected<std::vector<std::filesystem::path>, std::string> TrackStore::folders()
    {
        return collectRows(statements.folders, [](const Statement &row) { return fromUtf8(row.text(0)); });
    }

    std::expected<std::optional<FileStamp>, std::string> TrackStore::stamp(const std::filesystem::path &file)
    {
        Statement                                           &query  = statements.stamp.bind(1, toUtf8(file));
        const auto                                           row    = query.step();
        std::expected<std::optional<FileStamp>, std::string> result = std::nullopt;
        if (!row)
        {
            result = std::unexpected{row.error()};
        }
        else if (*row)
        {
            result = FileStamp{.modified = query.integer(0), .size = static_cast<std::uintmax_t>(query.integer(1))};
        }
        query.reset();
        return result;
    }

    std::expected<void, std::string> TrackStore::store(const std::filesystem::path &file, const FileStamp &stamp,
                                                       const TrackTags &tags)
    {
        const std::string file_name = toUtf8(file.filename());
        const TrackKeys   keys      = keysOf(tags, file_name);
        return statements.store.bind(1, toUtf8(file))
            .bind(2, file_name)
            .bind(3, stamp.modified)
            .bind(4, static_cast<std::int64_t>(stamp.size))
            .bind(5, tags.artist)
            .bind(6, tags.title)
            .bind(7, tags.album)
            .bind(8, tags.genre)
            .bind(9, std::int64_t{tags.year})
            .bind(10, tags.duration_seconds)
            .bind(11, keys.artist)
            .bind(12, keys.title)
            .bind(13, keys.genre)
            .bind(14, keys.any)
            .run();
    }

    std::expected<std::vector<api::LibraryTrack>, std::string> TrackStore::search(const api::LibraryQuery &query)
    {
        // Prepared per search: the conditions depend on which filters are set, and preparing is cheap next to the
        // search itself
        const SearchSql search    = buildSearch(query, track_columns);
        auto            statement = database.prepare(search.sql);
        if (!statement)
        {
            return std::unexpected{statement.error()};
        }
        for (std::size_t index = 0; index < search.parameters.size(); ++index)
        {
            statement->bind(static_cast<int>(index + 1), search.parameters[index]);
        }
        return collectRows(*statement, readTrack);
    }

    std::expected<std::size_t, std::string> TrackStore::trackCount()
    {
        Statement                              &query  = statements.count;
        const auto                              row    = query.step();
        std::expected<std::size_t, std::string> result = std::size_t{0};
        if (!row)
        {
            result = std::unexpected{row.error()};
        }
        else if (*row)
        {
            result = static_cast<std::size_t>(query.integer(0));
        }
        query.reset();
        return result;
    }
} // namespace djw::core::library
