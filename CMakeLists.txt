# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

cmake_minimum_required(VERSION 3.25)

# The version: src/player-ui/VERSION is the only place it is kept, e.g. 1.0.0 or 1.0.0-dev. project() takes only the
# numbers; DJW_VERSION is the whole string. Changing the file configures again.
set(djw_version_file ${CMAKE_CURRENT_SOURCE_DIR}/src/player-ui/VERSION)
file(STRINGS ${djw_version_file} DJW_VERSION LIMIT_COUNT 1)
if(NOT DJW_VERSION MATCHES "^([0-9]+\\.[0-9]+\\.[0-9]+)(-[0-9A-Za-z.]+)?$")
    message(FATAL_ERROR "${djw_version_file}: '${DJW_VERSION}' is not a version like 1.0.0 or 1.0.0-dev")
endif()
set(djw_version_numbers ${CMAKE_MATCH_1})
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${djw_version_file})

project(djw
    VERSION ${djw_version_numbers}
    DESCRIPTION "DJ-Workstation: lean, touch-first DJ software"
    LANGUAGES C CXX # C for miniaudio
)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(DjwOptions)
include(DjwSanitizers)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(_djw_tests_default ON)
else()
    set(_djw_tests_default OFF)
endif()

option(DJW_PLAYER_SDL "Build the SDL3 player (src/player-ui/sdl)" ON)
option(DJW_BUILD_TESTS "Build the tests" ${_djw_tests_default})
option(DJW_WITH_FFMPEG "Decode AAC/M4A with FFmpeg" OFF)
option(DJW_WITH_DVS "Build the DVS module (xwax timecoder)" OFF)
option(DJW_WARNINGS_AS_ERRORS "Treat compiler warnings as errors (CI)" OFF)

add_subdirectory(src/core)

if(DJW_PLAYER_SDL)
    add_subdirectory(src/player-ui/sdl)
endif()

if(DJW_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
