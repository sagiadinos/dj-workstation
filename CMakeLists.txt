# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

cmake_minimum_required(VERSION 3.25)

project(djw
    VERSION 0.1.0
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
