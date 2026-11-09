// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "api/version.hpp"

#include <doctest/doctest.h>

TEST_CASE("version matches the CMake project version")
{
    CHECK(djw::core::api::version() == DJW_EXPECTED_VERSION);
}
