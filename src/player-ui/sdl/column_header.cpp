// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "column_header.hpp"

#include "track_columns.hpp"

#include <utility>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr float padding    = 10.0F;
        constexpr float arrow_size = 8.0F;

        constexpr Color background{45, 45, 45};
        constexpr Color text_color{200, 200, 200};
        constexpr Color sorted_color{80, 160, 255};
    } // namespace

    void ColumnHeader::setSort(core::api::LibraryColumn column, bool sort_descending) noexcept
    {
        sort_by    = column;
        descending = sort_descending;
    }

    std::optional<core::api::LibraryColumn> ColumnHeader::takeTapped() noexcept
    {
        return std::exchange(tapped, std::nullopt);
    }

    std::optional<core::api::LibraryColumn> ColumnHeader::columnAt(Point position) const noexcept
    {
        if (!bounds().contains(position))
        {
            return std::nullopt;
        }
        for (const core::api::LibraryColumn column : track_columns)
        {
            if (columnArea(bounds(), column).contains(position))
            {
                return column;
            }
        }
        return std::nullopt;
    }

    void ColumnHeader::pointerDown(Point position)
    {
        pressed = columnAt(position);
    }

    void ColumnHeader::pointerUp(Point position)
    {
        // A tap: released on the column it went down on
        if (pressed && columnAt(position) == pressed)
        {
            tapped = pressed;
        }
        pressed.reset();
    }

    void ColumnHeader::draw(Canvas &canvas) const
    {
        canvas.fillRect(bounds(), background);
        for (const core::api::LibraryColumn column : track_columns)
        {
            const Rect  area   = columnArea(bounds(), column);
            const bool  sorted = column == sort_by;
            const float text_y = area.y + ((area.height - canvas.textHeight()) / 2);
            const float arrow  = sorted ? arrow_size + padding : 0.0F;
            canvas.textElided({area.x + padding, text_y}, columnName(column), sorted ? sorted_color : text_color,
                              area.width - (2 * padding) - arrow);
            if (sorted)
            {
                const std::string_view name   = columnName(column);
                const float            left   = area.x + padding + canvas.textWidth(name) + (padding / 2);
                const float            middle = area.y + (area.height / 2);
                const float            half   = arrow_size / 2;
                if (descending)
                {
                    canvas.fillTriangle({left, middle - half}, {left + arrow_size, middle - half},
                                        {left + half, middle + half}, sorted_color);
                }
                else
                {
                    canvas.fillTriangle({left, middle + half}, {left + arrow_size, middle + half},
                                        {left + half, middle - half}, sorted_color);
                }
            }
        }
    }
} // namespace djw::player_ui::sdl
