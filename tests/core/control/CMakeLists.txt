# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

add_executable(djw_core_control_tests
    main.cpp
    test_mapping.cpp
    test_controller.cpp
    test_controller_hub.cpp
)
djw_set_options(djw_core_control_tests TEST)
target_compile_definitions(djw_core_control_tests PRIVATE DJW_MAPPINGS_DIR="${PROJECT_SOURCE_DIR}/mappings")
target_link_libraries(djw_core_control_tests PRIVATE
    djw::core::control
    djw_test_support
    doctest::doctest
)

add_test(NAME core.control COMMAND djw_core_control_tests)
