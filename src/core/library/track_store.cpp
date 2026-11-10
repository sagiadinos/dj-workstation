// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library/track_store.hpp"

#include "library/search_query.hpp"
#include "library/text_fold.hpp"
#include "library/utf8_path.hpp"

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
                any_key    TEXT NOT NULL,
                -- 1 when the file was not found at the last check (dead link)
                missing    INTEGER NOT NULL DEFAULT 0
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

        // Version 2 had no missing flag
        constexpr std::string_view migration_to_3 =
            "ALTER TABLE tracks ADD COLUMN missing INTEGER NOT NULL DEFAULT 0; PRAGMA user_version = 3;";

        constexpr std::string_view track_columns = "t.id, t.path, t.artist, t.title, t.album, t.genre, t.year, "
                                                   "t.duration, t.missing, t.cue, t.bpm, t.analyzed";

        // Version 3 had no playlists
        constexpr std::string_view migration_to_4 = R"sql(
            CREATE TABLE playlists (
                id   INTEGER PRIMARY KEY,
                name TEXT    NOT NULL UNIQUE
            );
            CREATE TABLE playlist_entries (
                playlist_id INTEGER NOT NULL REFERENCES playlists (id) ON DELETE CASCADE,
                position    INTEGER NOT NULL,
                track_id    INTEGER NOT NULL REFERENCES tracks (id) ON DELETE CASCADE,
                PRIMARY KEY (playlist_id, position)
            );
            CREATE INDEX playlist_entries_by_track ON playlist_entries (track_id);
            PRAGMA user_version = 4;
        )sql";

        // Version 4 had no cue points; NULL is none
        constexpr std::string_view migration_to_5 = "ALTER TABLE tracks ADD COLUMN cue REAL; PRAGMA user_version = 5;";

        // Version 5 had no analysis: tempo (NULL: none), whether it ran, and the overview waveform in its own table
        // so that reading tracks never loads it
        constexpr std::string_view migration_to_6 = R"sql(
            ALTER TABLE tracks ADD COLUMN bpm REAL;
            ALTER TABLE tracks ADD COLUMN analyzed INTEGER NOT NULL DEFAULT 0;
            CREATE TABLE waveforms (
                track_id INTEGER PRIMARY KEY REFERENCES tracks (id) ON DELETE CASCADE,
                peaks    BLOB    NOT NULL
            );
            PRAGMA user_version = 6;
        )sql";

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

        /// The single number a count(*) query returns
        std::expected<std::size_t, std::string> countOf(Statement &query)
        {
            const auto counts =
                collectRows(query, [](const Statement &row) { return static_cast<std::size_t>(row.integer(0)); });
            if (!counts)
            {
                return std::unexpected{counts.error()};
            }
            return counts->empty() ? 0 : counts->front();
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
            .missing          = row.integer(8) != 0,
            .cue_seconds      = row.isNull(9) ? std::nullopt : std::optional{row.real(9)},
            .bpm              = row.isNull(10) ? std::nullopt : std::optional{row.real(10)},
            .analyzed         = row.integer(11) != 0,
        };
    }

    std::string_view trackColumns() noexcept
    {
        return track_columns;
    }

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
        return TrackStore{std::make_unique<SqliteDatabase>(std::move(*database)), std::move(*statements)};
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
            // A new database is created at version 3 and takes the same migration path as an existing one from there
            result = database.execute(std::string{schema} + "PRAGMA user_version = 3;");
            if (result)
            {
                result = migrate(database, 3);
            }
        }
        else if (*version < schema_version)
        {
            result = migrate(database, *version);
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

    std::expected<void, std::string> TrackStore::migrate(SqliteDatabase &database, std::int64_t from_version)
    {
        if (from_version < 2)
        {
            if (auto migrated = migrateToVersion2(database); !migrated)
            {
                return migrated;
            }
        }
        if (from_version < 3)
        {
            if (auto migrated = database.execute(migration_to_3); !migrated)
            {
                return migrated;
            }
        }
        if (from_version < 4)
        {
            if (auto migrated = database.execute(migration_to_4); !migrated)
            {
                return migrated;
            }
        }
        if (from_version < 5)
        {
            if (auto migrated = database.execute(migration_to_5); !migrated)
            {
                return migrated;
            }
        }
        return database.execute(migration_to_6);
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
        auto add_folder     = database.prepare("INSERT OR IGNORE INTO folders (path) VALUES (?)");
        auto folders        = database.prepare("SELECT path FROM folders ORDER BY path");
        auto stamp          = database.prepare("SELECT modified, size FROM tracks WHERE path = ?");
        auto store          = database.prepare(R"sql(
            INSERT INTO tracks (path, file_name, modified, size, artist, title, album, genre, year, duration,
                                artist_key, title_key, genre_key, any_key)
            VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14)
            ON CONFLICT (path) DO UPDATE SET
                file_name = excluded.file_name, modified = excluded.modified, size = excluded.size,
                artist = excluded.artist, title = excluded.title, album = excluded.album, genre = excluded.genre,
                year = excluded.year, duration = excluded.duration, artist_key = excluded.artist_key,
                title_key = excluded.title_key, genre_key = excluded.genre_key, any_key = excluded.any_key,
                bpm = CASE WHEN ?15 THEN bpm ELSE NULL END,
                analyzed = CASE WHEN ?15 THEN analyzed ELSE 0 END
        )sql");
        auto count          = database.prepare("SELECT count(*) FROM tracks");
        auto path           = database.prepare("SELECT path FROM tracks WHERE id = ?");
        auto files          = database.prepare("SELECT id, path, missing FROM tracks");
        auto set_missing    = database.prepare("UPDATE tracks SET missing = ? WHERE id = ?");
        auto missing_count  = database.prepare("SELECT count(*) FROM tracks WHERE missing = 1");
        auto remove_missing = database.prepare("DELETE FROM tracks WHERE missing = 1");
        auto id_of_path     = database.prepare("SELECT id FROM tracks WHERE path = ?");
        auto set_cue        = database.prepare("UPDATE tracks SET cue = ? WHERE id = ?");
        auto set_analysis   = database.prepare("UPDATE tracks SET bpm = ?, analyzed = 1 WHERE id = ?");
        auto set_waveform   = database.prepare("INSERT OR REPLACE INTO waveforms (track_id, peaks) VALUES (?, ?)");
        auto drop_waveform  = database.prepare(
            "DELETE FROM waveforms WHERE track_id IN (SELECT id FROM tracks WHERE path = ?)");
        auto waveform       = database.prepare("SELECT peaks FROM waveforms WHERE track_id = ?");
        auto is_analyzed    = database.prepare("SELECT analyzed FROM tracks WHERE id = ?");
        auto unanalyzed     = database.prepare("SELECT id, path, missing FROM tracks WHERE analyzed = 0 AND missing = 0");

        for (const auto *statement : {&add_folder, &folders, &stamp, &store, &count, &path, &files, &set_missing,
                                      &missing_count, &remove_missing, &id_of_path, &set_cue,
                                      &set_analysis, &set_waveform, &drop_waveform, &waveform, &is_analyzed,
                                      &unanalyzed})
        {
            if (!*statement)
            {
                return std::unexpected{statement->error()};
            }
        }
        return Statements{
            .add_folder     = std::move(*add_folder),
            .folders        = std::move(*folders),
            .stamp          = std::move(*stamp),
            .store          = std::move(*store),
            .count          = std::move(*count),
            .path           = std::move(*path),
            .files          = std::move(*files),
            .set_missing    = std::move(*set_missing),
            .missing_count  = std::move(*missing_count),
            .remove_missing = std::move(*remove_missing),
            .id_of_path     = std::move(*id_of_path),
            .set_cue        = std::move(*set_cue),
            .set_analysis   = std::move(*set_analysis),
            .set_waveform   = std::move(*set_waveform),
            .drop_waveform  = std::move(*drop_waveform),
            .waveform       = std::move(*waveform),
            .is_analyzed    = std::move(*is_analyzed),
            .unanalyzed     = std::move(*unanalyzed),
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
                                                       const TrackTags &tags, bool keep_analysis)
    {
        const std::string file_name = toUtf8(file.filename());
        const TrackKeys   keys      = keysOf(tags, file_name);
        if (!keep_analysis)
        {
            // The waveform goes with the analysis it belongs to
            if (auto dropped = statements.drop_waveform.bind(1, toUtf8(file)).run(); !dropped)
            {
                return dropped;
            }
        }
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
            .bind(15, std::int64_t{keep_analysis ? 1 : 0})
            .run();
    }

    std::expected<std::vector<api::LibraryTrack>, std::string> TrackStore::search(const api::LibraryQuery &query)
    {
        // Prepared per search: the conditions depend on which filters are set, and preparing is cheap next to the
        // search itself
        const SearchSql search    = buildSearch(query, track_columns);
        auto            statement = database->prepare(search.sql);
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
        return countOf(statements.count);
    }

    std::expected<std::size_t, std::string> TrackStore::missingCount()
    {
        return countOf(statements.missing_count);
    }

    std::expected<std::vector<StoredFile>, std::string> TrackStore::files()
    {
        return collectRows(statements.files, [](const Statement &row)
                           { return StoredFile{row.integer(0), fromUtf8(row.text(1)), row.integer(2) != 0}; });
    }

    std::expected<void, std::string> TrackStore::setMissing(std::int64_t track_id, bool missing)
    {
        return statements.set_missing.bind(1, std::int64_t{missing ? 1 : 0}).bind(2, track_id).run();
    }

    std::expected<void, std::string> TrackStore::setCue(std::int64_t track_id, std::optional<double> seconds)
    {
        Statement &update = statements.set_cue;
        if (seconds)
        {
            update.bind(1, *seconds);
        }
        else
        {
            update.bindNull(1);
        }
        return update.bind(2, track_id).run();
    }

    std::expected<void, std::string> TrackStore::setAnalysis(std::int64_t track_id, std::optional<double> bpm,
                                                             std::span<const std::uint8_t> waveform)
    {
        Statement &update = statements.set_analysis;
        if (bpm)
        {
            update.bind(1, *bpm);
        }
        else
        {
            update.bindNull(1);
        }
        if (auto updated = update.bind(2, track_id).run(); !updated)
        {
            return updated;
        }
        return statements.set_waveform.bind(1, track_id).bind(2, waveform).run();
    }

    std::expected<std::vector<std::uint8_t>, std::string> TrackStore::waveform(std::int64_t track_id)
    {
        auto rows = collectRows(statements.waveform.bind(1, track_id),
                                [](const Statement &row) { return row.blob(0); });
        if (!rows)
        {
            return std::unexpected{rows.error()};
        }
        return rows->empty() ? std::vector<std::uint8_t>{} : std::move(rows->front());
    }

    std::expected<bool, std::string> TrackStore::isAnalyzed(std::int64_t track_id)
    {
        auto rows = collectRows(statements.is_analyzed.bind(1, track_id),
                                [](const Statement &row) { return row.integer(0) != 0; });
        if (!rows)
        {
            return std::unexpected{rows.error()};
        }
        return !rows->empty() && rows->front();
    }

    std::expected<std::vector<StoredFile>, std::string> TrackStore::unanalyzed()
    {
        return collectRows(statements.unanalyzed, [](const Statement &row)
                           { return StoredFile{row.integer(0), fromUtf8(row.text(1)), row.integer(2) != 0}; });
    }

    std::expected<void, std::string> TrackStore::removeMissing()
    {
        return statements.remove_missing.run();
    }

    std::expected<std::optional<std::filesystem::path>, std::string> TrackStore::trackPath(std::int64_t track_id)
    {
        auto paths =
            collectRows(statements.path.bind(1, track_id), [](const Statement &row) { return fromUtf8(row.text(0)); });
        if (!paths)
        {
            return std::unexpected{paths.error()};
        }
        if (paths->empty())
        {
            return std::nullopt;
        }
        return std::move(paths->front());
    }

    std::expected<std::optional<std::int64_t>, std::string> TrackStore::trackId(const std::filesystem::path &file)
    {
        auto ids = collectRows(statements.id_of_path.bind(1, toUtf8(file)),
                               [](const Statement &row) { return row.integer(0); });
        if (!ids)
        {
            return std::unexpected{ids.error()};
        }
        return ids->empty() ? std::nullopt : std::optional{ids->front()};
    }
} // namespace djw::core::library
