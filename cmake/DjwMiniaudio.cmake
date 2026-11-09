# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

# miniaudio: decoding (WAV, FLAC, MP3) and later audio I/O.
#
# Unlike the other dependencies it is always downloaded: miniaudio ships no CMake package config, and its own
# CMakeLists.txt builds extras (libvorbis/libopus decoders, nodes) we do not use. So only miniaudio.c is compiled
# here; SOURCE_SUBDIR points to a directory without a CMakeLists.txt, so FetchContent only downloads.

include(FetchContent)

FetchContent_Declare(miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG        0.11.26
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  do-not-build
)
FetchContent_MakeAvailable(miniaudio)

find_package(Threads REQUIRED)

add_library(djw_miniaudio STATIC ${miniaudio_SOURCE_DIR}/miniaudio.c)
add_library(djw::miniaudio ALIAS djw_miniaudio)
target_include_directories(djw_miniaudio SYSTEM PUBLIC ${miniaudio_SOURCE_DIR})
# Parts of miniaudio djw does not use; they must be visible to every user of miniaudio.h too
target_compile_definitions(djw_miniaudio PUBLIC
    MA_NO_ENCODING
    MA_NO_GENERATION
    MA_NO_ENGINE
    MA_NO_RESOURCE_MANAGER
    MA_NO_NODE_GRAPH
)
target_link_libraries(djw_miniaudio PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
if(UNIX AND NOT APPLE)
    target_link_libraries(djw_miniaudio PUBLIC m)
endif()
