// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library/sqlite_database.hpp"

#include "sql_fault.hpp"
#include "temp_dir.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <span>
#include <vector>

using djw::core::library::SqliteDatabase;
using djw::tests::SqlFault;

namespace
{
    SqliteDatabase openIn(const djw::tests::TempDir &folder)
    {
        auto database = SqliteDatabase::open(folder.path() / "test.db");
        REQUIRE_MESSAGE(database.has_value(), database.error());
        REQUIRE(database->execute("CREATE TABLE numbers (value INTEGER)").has_value());
        return std::move(*database);
    }
} // namespace

TEST_CASE("SqliteDatabase: a fault makes commands, preparing and steps with its SQL fail, other SQL runs")
{
    const djw::tests::TempDir folder;
    SqliteDatabase            database = openIn(folder);
    auto                      select   = database.prepare("SELECT value FROM numbers");
    REQUIRE(select.has_value());

    const SqlFault fault{"numbers"};
    const auto     executed = database.execute("INSERT INTO numbers VALUES (1)");
    REQUIRE_FALSE(executed.has_value());
    CHECK(executed.error() == "database command failed: injected fault");
    const auto prepared = database.prepare("SELECT count(*) FROM numbers");
    REQUIRE_FALSE(prepared.has_value());
    CHECK(prepared.error() == "cannot prepare database query: injected fault");
    const auto stepped = select->step();
    REQUIRE_FALSE(stepped.has_value());
    CHECK(stepped.error() == "database query failed: injected fault");
    CHECK(fault.struck() == 3);

    CHECK(database.execute("SELECT 1").has_value());
    auto other = database.prepare("SELECT 2");
    REQUIRE(other.has_value());
    CHECK(other->step().value());
    CHECK(fault.struck() == 3);
}

TEST_CASE("SqliteDatabase: a fault with skip lets that many calls pass first, then fails every one")
{
    const djw::tests::TempDir folder;
    SqliteDatabase            database = openIn(folder);

    const SqlFault fault{"INSERT", 2};
    CHECK(database.execute("INSERT INTO numbers VALUES (1)").has_value());
    CHECK(database.execute("INSERT INTO numbers VALUES (2)").has_value());
    CHECK_FALSE(database.execute("INSERT INTO numbers VALUES (3)").has_value());
    CHECK_FALSE(database.execute("INSERT INTO numbers VALUES (4)").has_value());
    CHECK(fault.struck() == 2);
}

TEST_CASE("SqliteDatabase: a fault switched off, or set anew, fails nothing more and counts from 0")
{
    const djw::tests::TempDir folder;
    SqliteDatabase            database = openIn(folder);
    {
        const SqlFault fault{"INSERT"};
        CHECK_FALSE(database.execute("INSERT INTO numbers VALUES (1)").has_value());
    }
    CHECK(database.execute("INSERT INTO numbers VALUES (1)").has_value());
    CHECK(djw::core::library::testing::sqlFaults() == 0);

    const SqlFault fault{"DELETE"};
    CHECK(database.execute("INSERT INTO numbers VALUES (2)").has_value());
    CHECK(fault.struck() == 0);
}

TEST_CASE("SqliteDatabase: a database whose journal mode cannot be set is not opened")
{
    const djw::tests::TempDir folder;
    const SqlFault            fault{"PRAGMA journal_mode"};
    const auto                database = SqliteDatabase::open(folder.path() / "test.db");
    REQUIRE_FALSE(database.has_value());
    CHECK(database.error() == "database command failed: injected fault");
    CHECK(fault.struck() == 1);
}

TEST_CASE("SqliteDatabase: an empty blob bound after a full one is empty, not the old value")
{
    const djw::tests::TempDir folder;
    SqliteDatabase            database = openIn(folder);
    REQUIRE(database.execute("CREATE TABLE blobs (value BLOB)").has_value());
    auto insert = database.prepare("INSERT INTO blobs VALUES (?)");
    REQUIRE(insert.has_value());
    const std::vector<std::uint8_t> full{1, 2, 3};
    REQUIRE(insert->bind(1, std::span<const std::uint8_t>{full}).run().has_value());
    REQUIRE(insert->bind(1, std::span<const std::uint8_t>{}).run().has_value());

    auto select = database.prepare("SELECT value FROM blobs ORDER BY rowid");
    REQUIRE(select.has_value());
    REQUIRE(select->step().value());
    CHECK(select->blob(0) == full);
    REQUIRE(select->step().value());
    CHECK_FALSE(select->isNull(0));
    CHECK(select->blob(0).empty());
}
