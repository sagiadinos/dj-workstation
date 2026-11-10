# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

# The music library: SQLite database with full text search, folder scan and tags. Implements api::Library.

include(FetchContent)

# TagLib: taken from vcpkg or the system if available, otherwise downloaded and built static
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_BINDINGS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(taglib
    GIT_REPOSITORY    https://github.com/taglib/taglib.git
    GIT_TAG           v2.3.2
    GIT_SHALLOW       TRUE
    SYSTEM
    FIND_PACKAGE_ARGS 2.0 CONFIG
)
FetchContent_MakeAvailable(taglib)
# Built from source, the target is only called tag and has include directories for installs only: the headers
# used here lie in taglib/ and taglib/toolkit/, the generated taglib_config.h in the build directory
if(NOT TARGET TagLib::tag)
    target_include_directories(tag SYSTEM INTERFACE
        $<BUILD_INTERFACE:${taglib_SOURCE_DIR}/taglib>
        $<BUILD_INTERFACE:${taglib_SOURCE_DIR}/taglib/toolkit>
        $<BUILD_INTERFACE:${taglib_BINARY_DIR}>
    )
    add_library(TagLib::tag ALIAS tag)
endif()

add_library(djw_core_library STATIC
    sqlite_database.cpp
    file_name_tags.cpp
    tag_reader.cpp
    search_query.cpp
    track_store.cpp
    folder_scanner.cpp
    library_service.cpp
    open_library.cpp
)
add_library(djw::core::library ALIAS djw_core_library)
djw_set_options(djw_core_library)

target_include_directories(djw_core_library PUBLIC ${PROJECT_SOURCE_DIR}/src/core)
target_link_libraries(djw_core_library
    PUBLIC  djw::core::api
    PRIVATE djw::sqlite TagLib::tag Threads::Threads
)
