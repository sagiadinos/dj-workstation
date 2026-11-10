# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

# SQLite with FTS5 for the library.
#
# Like miniaudio it is always built from source: SQLite ships no CMake package config, and the system library may
# lack FTS5. The amalgamation is a single C file; SOURCE_SUBDIR points to a directory without a CMakeLists.txt, so
# FetchContent only downloads.

include(FetchContent)

FetchContent_Declare(sqlite
    URL           https://sqlite.org/2026/sqlite-amalgamation-3540100.zip
    URL_HASH      SHA256=0a9c8e34352dad46006aa82836d5383bff9dc6bc9ce5a60553c05aa30e6ad1f9
    SOURCE_SUBDIR do-not-build
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(sqlite)

find_package(Threads REQUIRED)

add_library(djw_sqlite STATIC ${sqlite_SOURCE_DIR}/sqlite3.c)
add_library(djw::sqlite ALIAS djw_sqlite)
target_include_directories(djw_sqlite SYSTEM PUBLIC ${sqlite_SOURCE_DIR})
target_compile_definitions(djw_sqlite PRIVATE
    SQLITE_ENABLE_FTS5
    SQLITE_THREADSAFE=1         # one connection per thread, see library/
    SQLITE_DQS=0                # no double-quoted string literals
    SQLITE_DEFAULT_MEMSTATUS=0
    SQLITE_OMIT_LOAD_EXTENSION  # no extensions loaded at runtime, no libdl needed
    SQLITE_OMIT_DEPRECATED
)
target_link_libraries(djw_sqlite PUBLIC Threads::Threads)
if(UNIX AND NOT APPLE)
    target_link_libraries(djw_sqlite PUBLIC m)
endif()
