// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_TRACK_COLUMNS_HPP
#define DJW_PLAYER_UI_SDL_TRACK_COLUMNS_HPP

#include "api/library.hpp"
#include "geometry.hpp"

#include <array>
#include <cstddef>
#include <string_view>

namespace djw::player_ui::sdl
{
    /// The columns of the track list, left to right
    inline constexpr std::array<core::api::LibraryColumn, 6> track_columns{
        core::api::LibraryColumn::Artist, core::api::LibraryColumn::Title, core::api::LibraryColumn::Genre,
        core::api::LibraryColumn::Year,   core::api::LibraryColumn::Bpm,   core::api::LibraryColumn::Duration};

    [[nodiscard]] std::string_view columnName(core::api::LibraryColumn column);

    /**
     * Where each column of track_columns lies within area: x and width from the column's share of the width, y and
     * height of area. Filter fields, header and rows use the same layout, so they line up.
     */
    [[nodiscard]] std::array<Rect, track_columns.size()> columnAreas(const Rect &area);

    /// Position of column in track_columns
    [[nodiscard]] std::size_t columnIndex(core::api::LibraryColumn column);

    /// The area of one column, see columnAreas
    [[nodiscard]] Rect columnArea(const Rect &area, core::api::LibraryColumn column);
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_TRACK_COLUMNS_HPP
