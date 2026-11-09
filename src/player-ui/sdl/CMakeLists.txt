# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

# SDL3 player. Talks to the core only through djw::core (src/core/api).
# SDL3 itself is added with the first window (M0, step 6).

add_executable(djw_player_sdl
    main.cpp
)
set_target_properties(djw_player_sdl PROPERTIES OUTPUT_NAME djw)
djw_set_options(djw_player_sdl)
target_link_libraries(djw_player_sdl PRIVATE djw::core)
