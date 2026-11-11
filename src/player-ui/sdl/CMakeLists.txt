# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

# SDL3 player. Talks to the core only through src/core/api (checked by tests/architecture).

include(FetchContent)

# SDL3: taken from vcpkg or the system if available, otherwise downloaded
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
    GIT_REPOSITORY    https://github.com/libsdl-org/SDL.git
    GIT_TAG           release-3.4.18
    GIT_SHALLOW       TRUE
    SYSTEM
    FIND_PACKAGE_ARGS 3.4 CONFIG
)
FetchContent_MakeAvailable(SDL3)

# SDL3_ttf for text: taken from vcpkg or the system if available, otherwise downloaded with FreeType and HarfBuzz
set(SDLTTF_VENDORED ON CACHE BOOL "" FORCE)
set(SDLTTF_HARFBUZZ ON CACHE BOOL "" FORCE)
set(SDLTTF_PLUTOSVG OFF CACHE BOOL "" FORCE)
set(SDLTTF_SAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3_ttf
    GIT_REPOSITORY    https://github.com/libsdl-org/SDL_ttf.git
    GIT_TAG           release-3.2.2
    GIT_SHALLOW       TRUE
    GIT_SUBMODULES    external/freetype external/harfbuzz
    SYSTEM
    FIND_PACKAGE_ARGS 3.2 CONFIG
)
FetchContent_MakeAvailable(SDL3_ttf)

# Everything except main(), so the tests can link it. Only the sdl_* files use SDL; the widgets draw on a Canvas.
add_library(djw_player_sdl_ui STATIC
    i18n/language.cpp
    i18n/message_format.cpp
    i18n/catalog.cpp
    i18n/catalog_en.cpp
    i18n/catalog_de.cpp
    i18n/catalog_fr.cpp
    i18n/catalog_es.cpp
    i18n/catalog_el.cpp
    i18n/catalog_ru.cpp
    i18n/translator.cpp
    pointer_router.cpp
    view_host.cpp
    pane_frame.cpp
    mdi_arrangement.cpp
    fader.cpp
    play_button.cpp
    time_format.cpp
    deck_view.cpp
    mixer_view.cpp
    text_field.cpp
    text_button.cpp
    slider.cpp
    check_box.cpp
    stepper.cpp
    drop_down.cpp
    layout.cpp
    settings.cpp
    devices_dialog.cpp
    look.cpp
    waveform_bar.cpp
    menu_bar.cpp
    context_menu.cpp
    track_columns.cpp
    column_header.cpp
    track_list.cpp
    library_view.cpp
    playlist_view.cpp
    command_line.cpp
    sdl_events.cpp
    sdl_text.cpp
    sdl_canvas.cpp
    sdl_window.cpp
    sdl_file_dialogs.cpp
    sdl_app.cpp
)
djw_set_options(djw_player_sdl_ui)
target_include_directories(djw_player_sdl_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(djw_player_sdl_ui PUBLIC djw::core SDL3::SDL3 SDL3_ttf::SDL3_ttf)

add_executable(djw_player_sdl
    main.cpp
)
set_target_properties(djw_player_sdl PROPERTIES OUTPUT_NAME djw)
djw_set_options(djw_player_sdl)
target_link_libraries(djw_player_sdl PRIVATE djw_player_sdl_ui)

# The UI font next to the binary, where SdlApp looks for it
add_custom_command(TARGET djw_player_sdl POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:djw_player_sdl>/fonts
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${PROJECT_SOURCE_DIR}/third_party/fonts/NotoSans-Regular.ttf
        ${PROJECT_SOURCE_DIR}/third_party/fonts/NotoSans-OFL.txt
        $<TARGET_FILE_DIR:djw_player_sdl>/fonts
    # The controller mappings that come with the program
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/mappings $<TARGET_FILE_DIR:djw_player_sdl>/mappings
)
