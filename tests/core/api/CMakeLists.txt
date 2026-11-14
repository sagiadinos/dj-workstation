# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

add_executable(djw_core_api_tests
    main.cpp
    test_version.cpp
    test_spsc_queue.cpp
    test_triple_buffer.cpp
    test_commands.cpp
)
djw_set_options(djw_core_api_tests TEST)
target_compile_definitions(djw_core_api_tests PRIVATE DJW_EXPECTED_VERSION="${DJW_VERSION}")
target_link_libraries(djw_core_api_tests PRIVATE
    djw::core::api
    doctest::doctest
    Threads::Threads
)

add_test(NAME core.api COMMAND djw_core_api_tests)
