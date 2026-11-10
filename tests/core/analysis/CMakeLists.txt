# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

add_executable(djw_core_analysis_tests
    main.cpp
    test_bpm_analyzer.cpp
    test_track_analysis.cpp
)
djw_set_options(djw_core_analysis_tests TEST)
target_compile_definitions(djw_core_analysis_tests PRIVATE
    DJW_TEST_AUDIO_DIR="${PROJECT_SOURCE_DIR}/tests/data/audio"
)
target_link_libraries(djw_core_analysis_tests PRIVATE
    djw::core::analysis
    doctest::doctest
)

add_test(NAME core.analysis COMMAND djw_core_analysis_tests)
