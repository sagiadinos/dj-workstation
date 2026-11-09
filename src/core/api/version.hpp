// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_API_VERSION_HPP
#define DJW_CORE_API_VERSION_HPP

#include <string_view>

namespace djw::core::api
{
    /// Version of the core as "major.minor.patch", taken from the CMake project version.
    std::string_view version() noexcept;
} // namespace djw::core::api

#endif // DJW_CORE_API_VERSION_HPP
