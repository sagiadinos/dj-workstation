# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

# Widget layer and event translation of the SDL player; no window is opened
add_executable(djw_player_sdl_tests
    main.cpp
    test_pointer_router.cpp
    test_widgets.cpp
    test_deck_view.cpp
    test_sdl_events.cpp
    test_text_field.cpp
    test_track_list.cpp
    test_library_view.cpp
    test_command_line.cpp
)
djw_set_options(djw_player_sdl_tests TEST)
target_include_directories(djw_player_sdl_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(djw_player_sdl_tests PRIVATE
    djw_player_sdl_ui
    doctest::doctest
)

add_test(NAME player_ui.sdl COMMAND djw_player_sdl_tests)
