// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_LIBRARY_SQLITE_DATABASE_HPP
#define DJW_CORE_LIBRARY_SQLITE_DATABASE_HPP

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

struct sqlite3;
struct sqlite3_stmt;

namespace djw::core::library
{
    /**
     * A prepared SQLite statement. Parameters are numbered from 1, result columns from 0, as in SQLite.
     *
     * Binding cannot fail for a valid index and type, so bind() reports nothing; errors of the statement itself
     * show up in step().
     */
    class Statement
    {
        public:
            Statement(Statement &&) noexcept            = default;
            Statement &operator=(Statement &&) noexcept = default;
            Statement(const Statement &)                = delete;
            Statement &operator=(const Statement &)     = delete;
            ~Statement()                                = default;

            Statement &bind(int index, std::string_view text);
            Statement &bind(int index, std::int64_t value);
            Statement &bind(int index, double value);
            Statement &bind(int index, std::span<const std::uint8_t> bytes);
            /// Binds NULL
            Statement &bindNull(int index);

            /// Runs one step: true when a row is available, false when done
            std::expected<bool, std::string> step();
            /// Runs the statement to the end, for statements without result rows
            std::expected<void, std::string> run();
            /// Ready to run again; bound values are kept until bound anew
            void reset();

            [[nodiscard]] std::string               text(int column) const;
            [[nodiscard]] std::int64_t              integer(int column) const;
            [[nodiscard]] double                    real(int column) const;
            [[nodiscard]] std::vector<std::uint8_t> blob(int column) const;
            /// The column of the current row is NULL
            [[nodiscard]] bool isNull(int column) const;

        private:
            friend class SqliteDatabase;

            struct Finalizer
            {
                    void operator()(sqlite3_stmt *statement) const noexcept;
            };

            Statement(sqlite3 *database, sqlite3_stmt *statement) noexcept;

            sqlite3                                 *database;
            std::unique_ptr<sqlite3_stmt, Finalizer> statement;
    };

    /// Runs query to the end and returns read(row) for every row; the query is reset in any case
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

    /// One SQLite connection. Use a connection from one thread only; several connections may share a file.
    class SqliteDatabase
    {
        public:
            /// Opens or creates the file, in WAL mode, waiting up to 5 s for a lock held by another connection
            static std::expected<SqliteDatabase, std::string> open(const std::filesystem::path &file);

            std::expected<void, std::string>      execute(std::string_view sql);
            std::expected<Statement, std::string> prepare(std::string_view sql);

        private:
            struct Closer
            {
                    void operator()(sqlite3 *database) const noexcept;
            };

            explicit SqliteDatabase(sqlite3 *database) noexcept : database{database} {}

            std::unique_ptr<sqlite3, Closer> database;
    };

    /// Text of the last error on the connection, prefixed with what failed
    std::string sqliteError(sqlite3 *database, std::string_view what);
} // namespace djw::core::library

#endif // DJW_CORE_LIBRARY_SQLITE_DATABASE_HPP
