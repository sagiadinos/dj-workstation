# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Niko Sagiadinos

# libdjw-core: every module is a static library, djw::core bundles them for the frontends.
# The core never links SDL or anything from src/player-ui.

add_subdirectory(api)

add_library(djw_core INTERFACE)
add_library(djw::core ALIAS djw_core)
target_link_libraries(djw_core INTERFACE djw::core::api)
