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
    test_columns.cpp
    test_library_view.cpp
    test_playlist_view.cpp
    test_command_line.cpp
    test_i18n.cpp
    test_controls.cpp
    test_mixer_view.cpp
    test_sdl_app.cpp
    test_menu_bar.cpp
)
djw_set_options(djw_player_sdl_tests TEST)
target_include_directories(djw_player_sdl_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(djw_player_sdl_tests PRIVATE
    djw_player_sdl_ui
    djw_test_support
    doctest::doctest
)

# The app test opens real SDL windows with the dummy video driver and needs the font next to the executable
add_custom_command(TARGET djw_player_sdl_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:djw_player_sdl_tests>/fonts
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${PROJECT_SOURCE_DIR}/third_party/fonts/NotoSans-Regular.ttf
        $<TARGET_FILE_DIR:djw_player_sdl_tests>/fonts
)

add_test(NAME player_ui.sdl COMMAND djw_player_sdl_tests)
