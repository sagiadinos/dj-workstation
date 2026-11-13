// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_TESTS_SUPPORT_SQL_FAULT_HPP
#define DJW_TESTS_SUPPORT_SQL_FAULT_HPP

#include "library/sqlite_database.hpp"

#include <string>
#include <utility>

namespace djw::tests
{
    /**
     * Makes the SQLite calls whose SQL contains sql_part fail while it lives, after the first skip of them (see
     * core::library::testing::failSql). Check struck() in the test: a fault that never struck means the SQL changed
     * and the test no longer tests what it says.
     */
    class SqlFault
    {
        public:
            explicit SqlFault(std::string sql_part, int skip = 0)
            {
                core::library::testing::failSql(std::move(sql_part), skip);
            }

            SqlFault(const SqlFault &)            = delete;
            SqlFault &operator=(const SqlFault &) = delete;
            SqlFault(SqlFault &&)                 = delete;
            SqlFault &operator=(SqlFault &&)      = delete;

            ~SqlFault() { core::library::testing::failSql({}); }

            /// How many calls failed so far
            [[nodiscard]] int struck() const { return core::library::testing::sqlFaults(); }
    };
} // namespace djw::tests

#endif // DJW_TESTS_SUPPORT_SQL_FAULT_HPP
