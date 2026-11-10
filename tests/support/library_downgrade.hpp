// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_TESTS_SUPPORT_LIBRARY_DOWNGRADE_HPP
#define DJW_TESTS_SUPPORT_LIBRARY_DOWNGRADE_HPP

#include "library/sqlite_database.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <string>

namespace djw::tests
{
    /**
     * Turns a library database of the current schema back into an older version, as that version had written it,
     * so migration tests start from a real old database. Each step undoes one migration; add a step with every new
     * schema version.
     */
    inline void downgradeLibrary(const std::filesystem::path &database_file, int version)
    {
        std::string sql;
        if (version < 4) // playlists
        {
            sql += "DROP TABLE playlist_entries; DROP TABLE playlists;";
        }
        if (version < 3) // missing flag
        {
            sql += "ALTER TABLE tracks DROP COLUMN missing;";
        }
        if (version < 2) // folded key columns
        {
            sql += "DROP INDEX tracks_by_artist_title;"
                   "ALTER TABLE tracks DROP COLUMN artist_key; ALTER TABLE tracks DROP COLUMN title_key;"
                   "ALTER TABLE tracks DROP COLUMN genre_key; ALTER TABLE tracks DROP COLUMN any_key;"
                   "CREATE INDEX tracks_by_artist_title ON tracks (artist COLLATE NOCASE, title COLLATE NOCASE);";
        }
        sql += "PRAGMA user_version = " + std::to_string(version);

        auto database = core::library::SqliteDatabase::open(database_file);
        REQUIRE_MESSAGE(database.has_value(), database.error());
        const auto downgraded = database->execute(sql);
        REQUIRE_MESSAGE(downgraded.has_value(), downgraded.error());
    }
} // namespace djw::tests

#endif // DJW_TESTS_SUPPORT_LIBRARY_DOWNGRADE_HPP
