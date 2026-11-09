# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

# Audio output devices. Knows nothing about the engine, it only calls an AudioCallback.
add_library(djw_core_audio_io STATIC
    miniaudio_device.cpp
)
add_library(djw::core::audio_io ALIAS djw_core_audio_io)
djw_set_options(djw_core_audio_io)

target_include_directories(djw_core_audio_io PUBLIC ${PROJECT_SOURCE_DIR}/src/core)
target_link_libraries(djw_core_audio_io PRIVATE djw::miniaudio)
