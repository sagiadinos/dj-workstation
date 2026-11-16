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
# Built from source, the target is only called tag and has include directories for installs only. An install puts all
# headers into one directory; in the source tree they lie in taglib/ and its subdirectories (toolkit/, mpeg/id3v1/,
# ...), so every directory with headers is added, the generated taglib_config.h lies in the build directory. A system
# TagLib is the installed layout: a header missing here fails only in the CI, not on a machine with TagLib installed.
if(NOT TARGET TagLib::tag)
    file(GLOB_RECURSE taglib_headers LIST_DIRECTORIES false "${taglib_SOURCE_DIR}/taglib/*.h")
    set(taglib_header_dirs "")
    foreach(header IN LISTS taglib_headers)
        get_filename_component(dir "${header}" DIRECTORY)
        list(APPEND taglib_header_dirs "$<BUILD_INTERFACE:${dir}>")
    endforeach()
    list(REMOVE_DUPLICATES taglib_header_dirs)
    target_include_directories(tag SYSTEM INTERFACE ${taglib_header_dirs} $<BUILD_INTERFACE:${taglib_BINARY_DIR}>)
    add_library(TagLib::tag ALIAS tag)
endif()

add_library(djw_core_library STATIC
    sqlite_database.cpp
    file_name_tags.cpp
    tag_reader.cpp
    tag_writer.cpp
    text_fold.cpp
    search_query.cpp
    track_store.cpp
    playlist_store.cpp
    m3u.cpp
    playlist_files.cpp
    folder_scanner.cpp
    missing_files.cpp
    missing_files.cpp
    library_service.cpp
    open_library.cpp
)
add_library(djw::core::library ALIAS djw_core_library)
djw_set_options(djw_core_library)

target_include_directories(djw_core_library PUBLIC ${PROJECT_SOURCE_DIR}/src/core)
target_link_libraries(djw_core_library
    PUBLIC  djw::core::api
    PRIVATE djw::core::analysis djw::sqlite TagLib::tag Threads::Threads
)
