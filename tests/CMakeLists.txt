# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

include(FetchContent)

find_package(Threads REQUIRED)

# doctest: taken from vcpkg or the system if available, otherwise downloaded
set(DOCTEST_WITH_TESTS OFF CACHE BOOL "" FORCE)
set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)

FetchContent_Declare(doctest
    GIT_REPOSITORY    https://github.com/doctest/doctest.git
    GIT_TAG           v2.4.12
    GIT_SHALLOW       TRUE
    SYSTEM            # warnings of doctest itself are not ours
    FIND_PACKAGE_ARGS CONFIG
)
# doctest 2.4.12 asks for an old CMake, which CMake 4 warns about; only for doctest
set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
FetchContent_MakeAvailable(doctest)
unset(CMAKE_POLICY_VERSION_MINIMUM)

# One test executable per module, mirroring src/
add_subdirectory(support)
add_subdirectory(core/api)
add_subdirectory(core/engine)
add_subdirectory(core/decode)
add_subdirectory(core/analysis)
add_subdirectory(core/audio_io)
add_subdirectory(core/runtime)
add_subdirectory(core/library)
add_subdirectory(integration)
if(DJW_PLAYER_SDL)
    add_subdirectory(player-ui/sdl)
endif()
add_subdirectory(architecture)
