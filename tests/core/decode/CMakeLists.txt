# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

add_executable(djw_core_decode_tests
    main.cpp
    test_sample_ring_buffer.cpp
    test_streaming_source.cpp
    test_miniaudio_decoder.cpp
    test_memory_track.cpp
    test_track_loader.cpp
)
djw_set_options(djw_core_decode_tests TEST)
target_compile_definitions(djw_core_decode_tests PRIVATE
    DJW_TEST_AUDIO_DIR="${PROJECT_SOURCE_DIR}/tests/data/audio"
)
target_link_libraries(djw_core_decode_tests PRIVATE
    djw::core::decode
    djw_test_support
    djw_test_allocation_counter
    doctest::doctest
    Threads::Threads
)

add_test(NAME core.decode COMMAND djw_core_decode_tests)
