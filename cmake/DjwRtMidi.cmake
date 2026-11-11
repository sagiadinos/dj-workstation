# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

# RtMidi: MIDI input and output of the controllers on Linux (ALSA), macOS (CoreMIDI) and Windows (WinMM). MIT-style
# license. Downloaded and built static from its one source file with the platform's API; the target is djw::rtmidi.
# Third-party code, so its warnings are off.

include(FetchContent)
FetchContent_Declare(rtmidi
    GIT_REPOSITORY https://github.com/thestk/rtmidi.git
    GIT_TAG        6.0.0
    SYSTEM
    SOURCE_SUBDIR  no-cmake # only the sources: its own CMake project is not used
)
FetchContent_MakeAvailable(rtmidi)

add_library(djw_rtmidi STATIC ${rtmidi_SOURCE_DIR}/RtMidi.cpp)
target_include_directories(djw_rtmidi SYSTEM PUBLIC ${rtmidi_SOURCE_DIR})
if(APPLE)
    target_compile_definitions(djw_rtmidi PRIVATE __MACOSX_CORE__)
    target_link_libraries(djw_rtmidi PRIVATE "-framework CoreMIDI" "-framework CoreAudio" "-framework CoreFoundation")
elseif(WIN32)
    target_compile_definitions(djw_rtmidi PRIVATE __WINDOWS_MM__)
    target_link_libraries(djw_rtmidi PRIVATE winmm)
elseif(UNIX)
    find_package(ALSA REQUIRED)
    find_package(Threads REQUIRED)
    target_compile_definitions(djw_rtmidi PRIVATE __LINUX_ALSA__)
    target_link_libraries(djw_rtmidi PRIVATE ALSA::ALSA Threads::Threads)
endif()
if(MSVC)
    target_compile_options(djw_rtmidi PRIVATE /w)
else()
    target_compile_options(djw_rtmidi PRIVATE -w)
endif()
set_target_properties(djw_rtmidi PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(djw::rtmidi ALIAS djw_rtmidi)
