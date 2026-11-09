# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

# Uses miniaudio's null backend: real timing and threads, no sound card needed
add_executable(djw_core_audio_io_tests
    main.cpp
    test_miniaudio_device.cpp
)
djw_set_options(djw_core_audio_io_tests TEST)
target_link_libraries(djw_core_audio_io_tests PRIVATE
    djw::core::audio_io
    doctest::doctest
)

add_test(NAME core.audio_io COMMAND djw_core_audio_io_tests)
