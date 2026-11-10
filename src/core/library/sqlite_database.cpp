// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "library/sqlite_database.hpp"

#include "library/utf8_path.hpp"

#include <sqlite3.h>

namespace djw::core::library
{
    namespace
    {
        constexpr int busy_timeout_ms = 5'000;
    } // namespace

    std::string sqliteError(sqlite3 *database, std::string_view what)
    {
        return std::string{what} + ": " + sqlite3_errmsg(database);
    }

    void Statement::Finalizer::operator()(sqlite3_stmt *statement) const noexcept
    {
        sqlite3_finalize(statement);
    }

    Statement::Statement(sqlite3 *database, sqlite3_stmt *statement) noexcept : database{database}, statement{statement}
    {
    }

    Statement &Statement::bind(int index, std::string_view text)
    {
        sqlite3_bind_text(statement.get(), index, text.data(), static_cast<int>(text.size()), SQLITE_TRANSIENT);
        return *this;
    }

    Statement &Statement::bind(int index, std::int64_t value)
    {
        sqlite3_bind_int64(statement.get(), index, value);
        return *this;
    }

    Statement &Statement::bind(int index, double value)
    {
        sqlite3_bind_double(statement.get(), index, value);
        return *this;
    }

    std::expected<bool, std::string> Statement::step()
    {
        const int result = sqlite3_step(statement.get());
        if (result == SQLITE_ROW)
        {
            return true;
        }
        if (result == SQLITE_DONE)
        {
            return false;
        }
        return std::unexpected{sqliteError(database, "database query failed")};
    }

    std::expected<void, std::string> Statement::run()
    {
        while (true)
        {
            const auto row = step();
            if (!row)
            {
                reset();
                return std::unexpected{row.error()};
            }
            if (!*row)
            {
                reset();
                return {};
            }
        }
    }

    void Statement::reset()
    {
        sqlite3_reset(statement.get());
    }

    std::string Statement::text(int column) const
    {
        // As blob: the same UTF-8 bytes as sqlite3_column_text, but as void *, so no reinterpret_cast is needed
        const void *bytes  = sqlite3_column_blob(statement.get(), column);
        const int   length = sqlite3_column_bytes(statement.get(), column);
        return bytes != nullptr ? std::string{static_cast<const char *>(bytes), static_cast<std::size_t>(length)}
                                : std::string{};
    }

    std::int64_t Statement::integer(int column) const
    {
        return sqlite3_column_int64(statement.get(), column);
    }

    double Statement::real(int column) const
    {
        return sqlite3_column_double(statement.get(), column);
    }

    void SqliteDatabase::Closer::operator()(sqlite3 *database) const noexcept
    {
        sqlite3_close_v2(database);
    }

    std::expected<SqliteDatabase, std::string> SqliteDatabase::open(const std::filesystem::path &file)
    {
        sqlite3  *handle = nullptr;
        const int result = sqlite3_open_v2(toUtf8(file).c_str(), &handle,
                                           SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX, nullptr);
        SqliteDatabase database{handle}; // closes the handle on every path, also when opening failed
        if (result != SQLITE_OK)
        {
            return std::unexpected{sqliteError(handle, "cannot open library database " + toUtf8(file))};
        }
        sqlite3_busy_timeout(handle, busy_timeout_ms);
        if (auto wal = database.execute("PRAGMA journal_mode = WAL; PRAGMA synchronous = NORMAL;"); !wal)
        {
            return std::unexpected{wal.error()};
        }
        return database;
    }

    std::expected<void, std::string> SqliteDatabase::execute(std::string_view sql)
    {
        const std::string text{sql};
        if (sqlite3_exec(database.get(), text.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK)
        {
            return std::unexpected{sqliteError(database.get(), "database command failed")};
        }
        return {};
    }

    std::expected<Statement, std::string> SqliteDatabase::prepare(std::string_view sql)
    {
        sqlite3_stmt *statement = nullptr;
        if (sqlite3_prepare_v2(database.get(), sql.data(), static_cast<int>(sql.size()), &statement, nullptr) !=
            SQLITE_OK)
        {
            return std::unexpected{sqliteError(database.get(), "cannot prepare database query")};
        }
        return Statement{database.get(), statement};
    }
} // namespace djw::core::library
