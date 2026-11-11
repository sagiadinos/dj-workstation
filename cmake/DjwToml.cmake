# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

# toml++: reads the controller mappings. MIT license, header only. Used without exceptions: a damaged mapping file
# is an error value, not a throw. The target is djw::toml.

include(FetchContent)
FetchContent_Declare(tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0
    GIT_SHALLOW    TRUE
    SYSTEM
    SOURCE_SUBDIR  no-cmake # only the headers: its own CMake project is not used
)
FetchContent_MakeAvailable(tomlplusplus)

add_library(djw_toml INTERFACE)
target_include_directories(djw_toml SYSTEM INTERFACE ${tomlplusplus_SOURCE_DIR}/include)
target_compile_definitions(djw_toml INTERFACE TOML_EXCEPTIONS=0)
add_library(djw::toml ALIAS djw_toml)
