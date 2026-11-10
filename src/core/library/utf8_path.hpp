// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_LIBRARY_UTF8_PATH_HPP
#define DJW_CORE_LIBRARY_UTF8_PATH_HPP

#include <filesystem>
#include <string>
#include <string_view>

namespace djw::core::library
{
    /// Paths are stored as UTF-8 in the database, on every platform
    inline std::string toUtf8(const std::filesystem::path &path)
    {
        const std::u8string text = path.u8string();
        return {text.begin(), text.end()};
    }

    inline std::filesystem::path fromUtf8(std::string_view text)
    {
        return std::filesystem::path{std::u8string{text.begin(), text.end()}};
    }
} // namespace djw::core::library

#endif // DJW_CORE_LIBRARY_UTF8_PATH_HPP
