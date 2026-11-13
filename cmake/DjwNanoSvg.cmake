# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

# nanosvg: reads and draws the logo (SVG paths with gradients) for the SDL player. zlib license, header only; the
# implementation is compiled in sdl_pictures.cpp. It has no releases, so a commit is pinned. The target is
# djw::nanosvg.

include(FetchContent)
FetchContent_Declare(nanosvg
    GIT_REPOSITORY https://github.com/memononen/nanosvg.git
    GIT_TAG        239e102ec2c691f2902e20ace2ed36ee4a35cfe6
    SYSTEM
    SOURCE_SUBDIR  no-cmake # only the headers: its own CMake project is not used
)
FetchContent_MakeAvailable(nanosvg)

add_library(djw_nanosvg INTERFACE)
target_include_directories(djw_nanosvg SYSTEM INTERFACE ${nanosvg_SOURCE_DIR}/src)
add_library(djw::nanosvg ALIAS djw_nanosvg)
