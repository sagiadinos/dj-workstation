// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "mdi_arrangement.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace djw::player_ui::sdl
{
    namespace
    {
        /// The most of the bottom the playlist takes next to the library
        constexpr float playlist_share = 0.4F;

        /// A pane of the top row: a player (index) or the crossfader
        struct RowPane
        {
                bool        mixer  = false;
                std::size_t player = 0;
        };
    } // namespace

    MdiArrangement arrangeMdi(const Rect &window, const MdiContent &content, const MdiSizes &sizes)
    {
        MdiArrangement result;
        result.players.resize(content.players);

        // The top row: player 1, the crossfader, the other players
        std::vector<RowPane> row;
        for (std::size_t player = 0; player < content.players; ++player)
        {
            row.push_back({.mixer = false, .player = player});
            if (player == 0 && content.mixer)
            {
                row.push_back({.mixer = true});
            }
        }
        if (content.players == 0 && content.mixer)
        {
            row.push_back({.mixer = true});
        }

        const bool  has_bottom = content.library || content.playlist;
        const float inner_x    = window.x + mdi_gap;
        const float inner_w    = std::max(0.0F, window.width - (2 * mdi_gap));
        float       bottom_top = window.y + mdi_gap;
        if (!row.empty())
        {
            float natural_width = mdi_gap * static_cast<float>(row.size() - 1);
            for (const RowPane &pane : row)
            {
                natural_width += pane.mixer ? sizes.mixer_width : sizes.player_width;
            }
            const float room_height =
                has_bottom ? window.height * mdi_row_share : std::max(0.0F, window.height - (2 * mdi_gap));
            // Only the panes shrink, the gaps between them stay
            const float gaps  = mdi_gap * static_cast<float>(row.size() - 1);
            const float scale = std::min(
                {1.0F, std::max(0.0F, inner_w - gaps) / (natural_width - gaps), room_height / sizes.player_height});
            const float width  = ((natural_width - gaps) * scale) + gaps;
            float       x      = inner_x + std::max(0.0F, (inner_w - width) / 2);
            const float y      = window.y + mdi_gap;
            const float height = sizes.player_height * scale;
            for (const RowPane &pane : row)
            {
                const float pane_width = (pane.mixer ? sizes.mixer_width : sizes.player_width) * scale;
                const Rect  area{x, y, pane_width, height};
                if (pane.mixer)
                {
                    result.mixer = area;
                }
                else
                {
                    result.players[pane.player] = area;
                }
                x += pane_width + mdi_gap;
            }
            bottom_top = y + height + mdi_gap;
        }

        if (has_bottom)
        {
            const float height = std::max(0.0F, window.y + window.height - mdi_gap - bottom_top);
            if (content.library && content.playlist)
            {
                const float list_width    = std::min(sizes.playlist_width, (inner_w - mdi_gap) * playlist_share);
                const float library_width = std::max(0.0F, inner_w - mdi_gap - list_width);
                result.library            = Rect{inner_x, bottom_top, library_width, height};
                result.playlist           = Rect{inner_x + library_width + mdi_gap, bottom_top, list_width, height};
            }
            else
            {
                (content.library ? result.library : result.playlist) = Rect{inner_x, bottom_top, inner_w, height};
            }
        }
        return result;
    }
    bool overlap(const Rect &first, const Rect &second) noexcept
    {
        return first.x < second.x + second.width && second.x < first.x + first.width &&
               first.y < second.y + second.height && second.y < first.y + first.height;
    }

    Rect keepInside(Rect area, const Rect &window) noexcept
    {
        area.width  = std::min(area.width, window.width);
        area.height = std::min(area.height, window.height);
        area.x      = std::clamp(area.x, window.x, window.x + window.width - area.width);
        area.y      = std::clamp(area.y, window.y, window.y + window.height - area.height);
        return area;
    }

    namespace
    {
        /// The nearest grid line to value, counted from origin
        float onGrid(float value, float origin, float grid) noexcept
        {
            return origin + (std::round((value - origin) / grid) * grid);
        }

        /// The last grid line at or before value, counted from origin
        float gridBefore(float value, float origin, float grid) noexcept
        {
            return origin + (std::floor((value - origin) / grid) * grid);
        }
    } // namespace

    Rect snapMove(const Rect &moved, const Rect &window, float grid) noexcept
    {
        Rect snapped = keepInside(moved, window);
        snapped.x    = onGrid(snapped.x, window.x, grid);
        snapped.y    = onGrid(snapped.y, window.y, grid);
        // Rounding may have pushed it over the right or bottom edge: then the grid line before
        if (snapped.x + snapped.width > window.x + window.width)
        {
            snapped.x = std::max(window.x, gridBefore(window.x + window.width - snapped.width, window.x, grid));
        }
        if (snapped.y + snapped.height > window.y + window.height)
        {
            snapped.y = std::max(window.y, gridBefore(window.y + window.height - snapped.height, window.y, grid));
        }
        return snapped;
    }

    Rect snapResize(const Rect &resized, const Rect &window, float grid) noexcept
    {
        Rect        snapped = resized;
        const float right   = std::min(onGrid(resized.x + resized.width, window.x, grid), window.x + window.width);
        const float bottom  = std::min(onGrid(resized.y + resized.height, window.y, grid), window.y + window.height);
        snapped.width       = std::max(right - resized.x, std::min(mdi_min_width, window.x + window.width - resized.x));
        snapped.height = std::max(bottom - resized.y, std::min(mdi_min_height, window.y + window.height - resized.y));
        return snapped;
    }

    std::optional<Rect> freePlace(float width, float height, const Rect &window, const std::vector<Rect> &taken,
                                  float grid)
    {
        constexpr int steps = 4; // full size, then smaller in steps down to half
        for (int step = 0; step <= steps; ++step)
        {
            const float scale   = 1.0F - (0.5F * static_cast<float>(step) / steps);
            const float w       = std::min(width * scale, window.width);
            const float h       = std::min(height * scale, window.height);
            const auto  columns = static_cast<int>(std::floor((window.width - w) / grid));
            const auto  rows    = static_cast<int>(std::floor((window.height - h) / grid));
            for (int row = 0; row <= rows; ++row)
            {
                for (int column = 0; column <= columns; ++column)
                {
                    const Rect place{window.x + (static_cast<float>(column) * grid),
                                     window.y + (static_cast<float>(row) * grid), w, h};
                    if (std::ranges::none_of(taken, [&place](const Rect &other) { return overlap(place, other); }))
                    {
                        return place;
                    }
                }
            }
        }
        return std::nullopt;
    }

    std::optional<Rect> largestFree(const Rect &window, const std::vector<Rect> &taken, float grid)
    {
        const auto columns = static_cast<std::size_t>(std::max(0.0F, std::floor(window.width / grid)));
        const auto rows    = static_cast<std::size_t>(std::max(0.0F, std::floor(window.height / grid)));
        if (columns == 0 || rows == 0)
        {
            return std::nullopt;
        }
        // The cells a taken area covers more than at an edge
        std::vector<bool> blocked(columns * rows, false);
        const auto        cell = [grid](float offset, float size, std::size_t count, bool end)
        {
            const float position = (end ? std::ceil((offset + size) / grid) : std::floor(offset / grid));
            return static_cast<std::size_t>(std::clamp(position, 0.0F, static_cast<float>(count)));
        };
        for (const Rect &area : taken)
        {
            const std::size_t first_column = cell(area.x - window.x, 0.0F, columns, false);
            const std::size_t end_column   = cell(area.x - window.x, area.width, columns, true);
            const std::size_t first_row    = cell(area.y - window.y, 0.0F, rows, false);
            const std::size_t end_row      = cell(area.y - window.y, area.height, rows, true);
            for (std::size_t row = first_row; row < end_row; ++row)
            {
                for (std::size_t column = first_column; column < end_column; ++column)
                {
                    blocked[(row * columns) + column] = true;
                }
            }
        }
        // Row by row, the free cells above each column as a histogram; its biggest rectangle with a stack
        std::vector<std::size_t> heights(columns, 0);
        std::size_t              best = 0;
        Rect                     found;
        for (std::size_t row = 0; row < rows; ++row)
        {
            for (std::size_t column = 0; column < columns; ++column)
            {
                heights[column] = blocked[(row * columns) + column] ? 0 : heights[column] + 1;
            }
            std::vector<std::size_t> stack;
            for (std::size_t column = 0; column <= columns; ++column)
            {
                const std::size_t height = column < columns ? heights[column] : 0;
                while (!stack.empty() && heights[stack.back()] >= height)
                {
                    const std::size_t top = heights[stack.back()];
                    stack.pop_back();
                    const std::size_t left  = stack.empty() ? 0 : stack.back() + 1;
                    const std::size_t width = column - left;
                    if (top * width > best)
                    {
                        best  = top * width;
                        found = {window.x + (static_cast<float>(left) * grid),
                                 window.y + (static_cast<float>(row + 1 - top) * grid),
                                 static_cast<float>(width) * grid, static_cast<float>(top) * grid};
                    }
                }
                stack.push_back(column);
            }
        }
        if (best == 0)
        {
            return std::nullopt;
        }
        return found;
    }
} // namespace djw::player_ui::sdl
