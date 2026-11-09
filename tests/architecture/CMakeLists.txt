# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

set(check ${CMAKE_CURRENT_SOURCE_DIR}/check_includes.cmake)
set(fixtures ${CMAKE_CURRENT_SOURCE_DIR}/fixtures)

# The real check on the project sources
add_test(NAME architecture.includes
    COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${PROJECT_SOURCE_DIR} -P ${check}
)

# The check itself, on small source trees with known violations
add_test(NAME architecture.includes.fixture_clean
    COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${fixtures}/clean -P ${check}
)
add_test(NAME architecture.includes.fixture_core_includes_sdl
    COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${fixtures}/core_includes_sdl -P ${check}
)
set_tests_properties(architecture.includes.fixture_core_includes_sdl PROPERTIES
    PASS_REGULAR_EXPRESSION "core must not include SDL"
)
add_test(NAME architecture.includes.fixture_player_includes_engine
    COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${fixtures}/player_includes_engine -P ${check}
)
set_tests_properties(architecture.includes.fixture_player_includes_engine PROPERTIES
    PASS_REGULAR_EXPRESSION "player-ui may include from the core only api/"
)
