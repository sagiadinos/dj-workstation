# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

# Tests across modules: engine, decoder and real files together, still without an audio device
add_executable(djw_integration_tests
    main.cpp
    test_file_playback.cpp
)
djw_set_options(djw_integration_tests TEST)
target_compile_definitions(djw_integration_tests PRIVATE
    DJW_TEST_AUDIO_DIR="${PROJECT_SOURCE_DIR}/tests/data/audio"
)
target_link_libraries(djw_integration_tests PRIVATE
    djw::core::engine
    djw::core::decode
    doctest::doctest
)

add_test(NAME integration COMMAND djw_integration_tests)
