# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

add_library(djw_core_engine STATIC
    deck.cpp
    mixer_channel.cpp
    engine.cpp
    reclaimer.cpp
)
add_library(djw::core::engine ALIAS djw_core_engine)
djw_set_options(djw_core_engine)

target_include_directories(djw_core_engine PUBLIC ${PROJECT_SOURCE_DIR}/src/core)
target_link_libraries(djw_core_engine PUBLIC djw::core::api)
