# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

add_executable(djw_core_runtime_tests
    main.cpp
    test_runtime_core.cpp
)
djw_set_options(djw_core_runtime_tests TEST)
target_compile_definitions(djw_core_runtime_tests PRIVATE
    DJW_TEST_AUDIO_DIR="${PROJECT_SOURCE_DIR}/tests/data/audio"
)
target_link_libraries(djw_core_runtime_tests PRIVATE
    djw::core::runtime
    djw_test_support
    doctest::doctest
)

add_test(NAME core.runtime COMMAND djw_core_runtime_tests)
