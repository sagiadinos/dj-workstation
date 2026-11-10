# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

# Analysis of tracks: tempo (and later waveform peaks). Runs on analysis threads, never on the audio thread.
add_library(djw_core_analysis STATIC
    bpm_analyzer.cpp
    track_analysis.cpp
)
add_library(djw::core::analysis ALIAS djw_core_analysis)
djw_set_options(djw_core_analysis)

target_include_directories(djw_core_analysis PUBLIC ${PROJECT_SOURCE_DIR}/src/core)
target_link_libraries(djw_core_analysis
    PUBLIC  djw::core::decode
    PRIVATE djw::qmdsp
)
