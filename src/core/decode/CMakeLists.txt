# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

# Decoding and streaming of audio files. StreamingSource implements engine::AudioSource, so decode depends on the
# engine interface; the engine itself knows nothing about decoders.
add_library(djw_core_decode STATIC
    miniaudio_decoder.cpp
    sample_ring_buffer.cpp
    streaming_source.cpp
    memory_track.cpp
    first_sound.cpp
    track_loader.cpp
)
add_library(djw::core::decode ALIAS djw_core_decode)
djw_set_options(djw_core_decode)

target_include_directories(djw_core_decode PUBLIC ${PROJECT_SOURCE_DIR}/src/core)
target_link_libraries(djw_core_decode
    PUBLIC  djw::core::engine
    PRIVATE djw::miniaudio Threads::Threads
)
