// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_TESTS_SUPPORT_TEMP_DIR_HPP
#define DJW_TESTS_SUPPORT_TEMP_DIR_HPP

#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>
#include <system_error>

namespace djw::tests
{
    /// A fresh, empty folder below the system's temp folder, removed with everything in it on destruction
    class TempDir
    {
        public:
            TempDir()
            {
                static std::atomic<int> counter{0};
                const auto              stamp = std::chrono::steady_clock::now().time_since_epoch().count();
                folder = std::filesystem::temp_directory_path() /
                         ("djw-test-" + std::to_string(stamp) + "-" + std::to_string(counter.fetch_add(1)));
                std::filesystem::create_directories(folder);
            }

            TempDir(const TempDir &)            = delete;
            TempDir &operator=(const TempDir &) = delete;
            TempDir(TempDir &&)                 = delete;
            TempDir &operator=(TempDir &&)      = delete;

            ~TempDir()
            {
                std::error_code ignored;
                std::filesystem::remove_all(folder, ignored);
            }

            [[nodiscard]] const std::filesystem::path &path() const noexcept { return folder; }

        private:
            std::filesystem::path folder;
    };
} // namespace djw::tests

#endif // DJW_TESTS_SUPPORT_TEMP_DIR_HPP
