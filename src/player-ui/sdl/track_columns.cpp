// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "track_columns.hpp"

#include <algorithm>
#include <cstddef>

namespace djw::player_ui::sdl
{
    namespace
    {
        /// Share of the width per column, in the order of track_columns
        constexpr std::array<float, track_columns.size()> shares{0.28F, 0.31F, 0.18F, 0.12F, 0.11F};

    } // namespace

    std::size_t columnIndex(core::api::LibraryColumn column)
    {
        return static_cast<std::size_t>(std::ranges::find(track_columns, column) - track_columns.begin());
    }

    std::string_view columnName(core::api::LibraryColumn column)
    {
        switch (column)
        {
            case core::api::LibraryColumn::Title:
                return "Title";
            case core::api::LibraryColumn::Genre:
                return "Genre";
            case core::api::LibraryColumn::Year:
                return "Year";
            case core::api::LibraryColumn::Duration:
                return "Time";
            case core::api::LibraryColumn::Artist:
                break;
        }
        return "Artist";
    }

    std::array<Rect, track_columns.size()> columnAreas(const Rect &area)
    {
        std::array<Rect, track_columns.size()> areas{};
        float                                  x = area.x;
        for (std::size_t index = 0; index < areas.size(); ++index)
        {
            // The last column takes what is left, so rounding never leaves a gap at the right edge
            const float width = index + 1 == areas.size() ? area.x + area.width - x : area.width * shares[index];
            areas[index]      = {x, area.y, width, area.height};
            x += width;
        }
        return areas;
    }

    Rect columnArea(const Rect &area, core::api::LibraryColumn column)
    {
        return columnAreas(area)[columnIndex(column)];
    }
} // namespace djw::player_ui::sdl
