# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

# Signalsmith Stretch: time stretching for keylock, one of the stretchers to choose from. MIT, as is its helper
# library Signalsmith Linear (FFT and STFT), which its CMakeLists fetches. Header-only; the target is
# djw::signalsmith_stretch.

include(FetchContent)
FetchContent_Declare(signalsmith_stretch
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
    GIT_TAG        1.4.0
    GIT_SHALLOW    TRUE
    SYSTEM
)
FetchContent_MakeAvailable(signalsmith_stretch)

add_library(djw_signalsmith_stretch INTERFACE)
target_link_libraries(djw_signalsmith_stretch INTERFACE signalsmith-stretch)
add_library(djw::signalsmith_stretch ALIAS djw_signalsmith_stretch)
