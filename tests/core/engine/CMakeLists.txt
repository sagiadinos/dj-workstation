# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

add_executable(djw_core_engine_tests
    main.cpp
    test_deck.cpp
    test_mixer_channel.cpp
    test_engine.cpp
    test_reclaimer.cpp
    test_resampler.cpp
    test_render_golden.cpp
)
djw_set_options(djw_core_engine_tests TEST)
target_compile_definitions(djw_core_engine_tests PRIVATE
    DJW_TEST_GOLDEN_DIR="${PROJECT_SOURCE_DIR}/tests/data/golden"
)
target_link_libraries(djw_core_engine_tests PRIVATE
    djw::core::engine
    djw_test_support
    djw_test_allocation_counter
    doctest::doctest
    Threads::Threads
)

add_test(NAME core.engine COMMAND djw_core_engine_tests)
