# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

# Controllers: MIDI input (RtMidi), the TOML mappings and the events they give. Runs on MIDI threads and the
# frontend's thread, never on the audio thread.
add_library(djw_core_control STATIC
    mapping.cpp
    controller.cpp
    controller_hub.cpp
    mixxx_import.cpp
    rtmidi_system.cpp
)
add_library(djw::core::control ALIAS djw_core_control)
djw_set_options(djw_core_control)

target_include_directories(djw_core_control PUBLIC ${PROJECT_SOURCE_DIR}/src/core)
target_link_libraries(djw_core_control
    PRIVATE djw::toml djw::rtmidi
)
