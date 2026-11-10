# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

# SoundTouch: keylock (tempo change without pitch change). LGPL-2.1.
#
# Taken from vcpkg or the system if available, otherwise downloaded and built static; the target is djw::soundtouch
# either way. The package config of some distributions lists include paths that do not exist, so the installed
# library and header are looked up directly. The header lies in soundtouch/ in an install and in the include folder
# of a source build (time_stretcher.cpp handles both).

option(DJW_SYSTEM_SOUNDTOUCH "Use an installed SoundTouch if there is one" ON)

if(DJW_SYSTEM_SOUNDTOUCH)
    find_path(DJW_SOUNDTOUCH_INCLUDE_DIR NAMES soundtouch/SoundTouch.h SoundTouch.h)
    find_library(DJW_SOUNDTOUCH_LIBRARY NAMES SoundTouch)
endif()

if(DJW_SOUNDTOUCH_INCLUDE_DIR AND DJW_SOUNDTOUCH_LIBRARY)
    message(STATUS "Found SoundTouch: ${DJW_SOUNDTOUCH_LIBRARY}")
    add_library(djw_soundtouch INTERFACE)
    target_include_directories(djw_soundtouch SYSTEM INTERFACE ${DJW_SOUNDTOUCH_INCLUDE_DIR})
    target_link_libraries(djw_soundtouch INTERFACE ${DJW_SOUNDTOUCH_LIBRARY})
else()
    include(FetchContent)
    set(SOUNDSTRETCH OFF CACHE BOOL "" FORCE)
    set(SOUNDTOUCH_DLL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SoundTouch
        GIT_REPOSITORY https://codeberg.org/soundtouch/soundtouch.git
        GIT_TAG        2.4.1
        GIT_SHALLOW    TRUE
        SYSTEM
    )
    FetchContent_MakeAvailable(SoundTouch)
    add_library(djw_soundtouch INTERFACE)
    target_link_libraries(djw_soundtouch INTERFACE SoundTouch)
endif()
add_library(djw::soundtouch ALIAS djw_soundtouch)
