# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

add_executable(djw_core_library_tests
    main.cpp
    test_file_name_tags.cpp
    test_text_fold.cpp
    test_search_query.cpp
    test_tag_reader.cpp
    test_tag_writer.cpp
    test_track_store.cpp
    test_playlist_store.cpp
    test_m3u.cpp
    test_playlist_files.cpp
    test_folder_scanner.cpp
    test_missing_files.cpp
    test_library_service.cpp
)
djw_set_options(djw_core_library_tests TEST)
target_compile_definitions(djw_core_library_tests PRIVATE
    DJW_TEST_AUDIO_DIR="${PROJECT_SOURCE_DIR}/tests/data/audio"
)
target_link_libraries(djw_core_library_tests PRIVATE
    djw::core::library
    djw_test_support
    doctest::doctest
    Threads::Threads
)

add_test(NAME core.library COMMAND djw_core_library_tests)
