# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

# Checks the include rules from Architecture.md on the sources below SOURCE_DIR/src:
# - src/core must not include anything from src/player-ui and no SDL headers
# - src/player-ui may include from the core only src/core/api
#
# Usage: cmake -DSOURCE_DIR=<project root> -P check_includes.cmake
#
# The check is textual. The hard boundary is the target separation in CMake (the core never links SDL or a
# frontend); this script catches includes that slip through anyway.

cmake_minimum_required(VERSION 3.25)

if(NOT SOURCE_DIR OR NOT IS_DIRECTORY "${SOURCE_DIR}/src/core")
    message(FATAL_ERROR "SOURCE_DIR must point to the project root, got '${SOURCE_DIR}'")
endif()

set(core_dir "${SOURCE_DIR}/src/core")
set(player_dir "${SOURCE_DIR}/src/player-ui")
set(source_globs *.hpp *.cpp *.h *.c *.inl)
set(include_regex "^[ \t]*#[ \t]*include[ \t]*[<\"]([^>\"]+)[>\"]")

set(violations "")

# Returns the include paths of a file in out_var
function(read_includes file out_var)
    file(STRINGS "${file}" lines REGEX "${include_regex}")
    set(includes "")
    foreach(line IN LISTS lines)
        if(line MATCHES "${include_regex}")
            list(APPEND includes "${CMAKE_MATCH_1}")
        endif()
    endforeach()
    set(${out_var} "${includes}" PARENT_SCOPE)
endfunction()

macro(add_violation file include reason)
    file(RELATIVE_PATH rel "${SOURCE_DIR}" "${file}")
    list(APPEND violations "${rel}: #include ${include}: ${reason}")
endmacro()

# Frontend directories (src/player-ui/<frontend>), used to detect core includes of frontend headers
set(frontends "")
if(IS_DIRECTORY "${player_dir}")
    file(GLOB entries LIST_DIRECTORIES true "${player_dir}/*")
    foreach(entry IN LISTS entries)
        if(IS_DIRECTORY "${entry}")
            list(APPEND frontends "${entry}")
        endif()
    endforeach()
endif()

# Rules for the core
list(TRANSFORM source_globs PREPEND "${core_dir}/" OUTPUT_VARIABLE core_globs)
file(GLOB_RECURSE core_files ${core_globs})
foreach(file IN LISTS core_files)
    read_includes("${file}" includes)
    foreach(include IN LISTS includes)
        if(include MATCHES "^SDL")
            add_violation("${file}" "${include}" "core must not include SDL")
        elseif(include MATCHES "(^|/)player-ui/")
            add_violation("${file}" "${include}" "core must not include player-ui")
        else()
            foreach(frontend IN LISTS frontends)
                if(EXISTS "${frontend}/${include}" AND NOT EXISTS "${core_dir}/${include}")
                    add_violation("${file}" "${include}" "core must not include player-ui")
                endif()
            endforeach()
        endif()
    endforeach()
endforeach()

# Rules for the frontends
list(TRANSFORM source_globs PREPEND "${player_dir}/" OUTPUT_VARIABLE player_globs)
file(GLOB_RECURSE player_files ${player_globs})
foreach(file IN LISTS player_files)
    read_includes("${file}" includes)
    foreach(include IN LISTS includes)
        if(include MATCHES "(^|/)core/" AND NOT include MATCHES "(^|/)core/api/")
            add_violation("${file}" "${include}" "player-ui may include from the core only api/")
        elseif(EXISTS "${core_dir}/${include}" AND NOT include MATCHES "^api/")
            add_violation("${file}" "${include}" "player-ui may include from the core only api/")
        endif()
    endforeach()
endforeach()

list(LENGTH core_files core_count)
list(LENGTH player_files player_count)

if(violations)
    list(JOIN violations "\n  " text)
    message(FATAL_ERROR "Include rules violated:\n  ${text}")
endif()

message(STATUS "Include rules ok (${core_count} core files, ${player_count} player-ui files)")
