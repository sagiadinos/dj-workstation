# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

# Helpers shared by the test executables: test audio sources, WAV files, golden comparison
add_library(djw_test_support STATIC
    wav_file.cpp
    golden.cpp
)
djw_set_options(djw_test_support TEST)
target_include_directories(djw_test_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(djw_test_support PUBLIC djw::core::engine)

# Replaces the global operator new/delete, so only tests that check allocations link it
add_library(djw_test_allocation_counter OBJECT
    allocation_counter.cpp
)
djw_set_options(djw_test_allocation_counter TEST)
target_include_directories(djw_test_allocation_counter PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
