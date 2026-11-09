// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "api/version.hpp"

namespace djw::core::api
{
    std::string_view version() noexcept
    {
        return DJW_VERSION;
    }
} // namespace djw::core::api
