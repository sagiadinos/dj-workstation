# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

add_library(djw_core_api STATIC
    version.cpp
)
add_library(djw::core::api ALIAS djw_core_api)
djw_set_options(djw_core_api)

# Project headers are included relative to src/core, e.g. "api/version.hpp"
target_include_directories(djw_core_api PUBLIC ${PROJECT_SOURCE_DIR}/src/core)
target_compile_definitions(djw_core_api PRIVATE DJW_VERSION="${DJW_VERSION}")
