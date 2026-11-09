# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

# Composition of the core: implements api::Core on top of engine, decode and audio_io.
add_library(djw_core_runtime STATIC
    runtime_core.cpp
    create_core.cpp
)
add_library(djw::core::runtime ALIAS djw_core_runtime)
djw_set_options(djw_core_runtime)

target_include_directories(djw_core_runtime PUBLIC ${PROJECT_SOURCE_DIR}/src/core)
target_link_libraries(djw_core_runtime PUBLIC
    djw::core::api
    djw::core::engine
    djw::core::decode
    djw::core::audio_io
)
