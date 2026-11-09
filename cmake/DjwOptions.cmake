# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

# Common compile settings for all targets of djw:
# - C++23 without compiler extensions
# - warnings as in Architecture.md, as errors with DJW_WARNINGS_AS_ERRORS
#
# Test executables pass TEST: with Clang, doctest includes <ciso646>, which libstdc++ >= 15 marks deprecated with
# #warning.

function(djw_set_options target)
    cmake_parse_arguments(PARSE_ARGV 1 arg "TEST" "" "")

    target_compile_features(${target} PUBLIC cxx_std_23)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
        if(DJW_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(DJW_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
        if(arg_TEST AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(${target} PRIVATE "-Wno-#warnings")
        endif()
    endif()
endfunction()
