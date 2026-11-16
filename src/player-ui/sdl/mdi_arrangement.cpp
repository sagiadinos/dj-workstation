// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "mdi_arrangement.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>
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

        // The top row: player 1, the middle column (the crossfader, vertical beat waveforms above it), the other
        // players
        const bool           beats_across = content.beats && !content.beats_vertical;
        const bool           beats_column = content.beats && content.beats_vertical;
        const bool           has_middle   = content.mixer || beats_column;
        const float          middle_width = content.mixer ? sizes.mixer_width : sizes.beats_depth;
        std::vector<RowPane> row;
        for (std::size_t player = 0; player < content.players; ++player)
        {
            row.push_back({.mixer = false, .player = player});
            if (player == 0 && has_middle)
            {
                row.push_back({.mixer = true});
            }
        }
        if (content.players == 0 && has_middle)
        {
            row.push_back({.mixer = true});
        }

        const bool  has_lists = content.library || content.playlist;
        const float inner_x   = window.x + mdi_gap;
        const float inner_w   = std::max(0.0F, window.width - (2 * mdi_gap));
        float       top       = window.y + mdi_gap;
        float       consumed  = 0.0F; // of the height, by the beat waveforms on top
        if (beats_across)
        {
            // A strip across the top, at most a third of the window
            const float strip = std::max(0.0F, std::min(sizes.beats_depth, (window.height - (2 * mdi_gap)) / 3));
            result.beats      = Rect{inner_x, top, inner_w, strip};
            consumed          = strip + mdi_gap;
            top += consumed;
        }
        float bottom_top = top;
        if (!row.empty())
        {
            float natural_width = mdi_gap * static_cast<float>(row.size() - 1);
            for (const RowPane &pane : row)
            {
                natural_width += pane.mixer ? middle_width : sizes.player_width;
            }
            // Vertical beat waveforms and the crossfader below them share a player's height: the column ends flush
            // with the players
            const float rest        = window.height - consumed;
            const float room_height = has_lists ? rest * mdi_row_share : std::max(0.0F, rest - (2 * mdi_gap));
            // Only the panes shrink, the gaps between them stay
            const float gaps   = mdi_gap * static_cast<float>(row.size() - 1);
            const float scale  = std::max(0.0F, std::min({1.0F, std::max(0.0F, inner_w - gaps) / (natural_width - gaps),
                                                          room_height / sizes.player_height}));
            const float width  = ((natural_width - gaps) * scale) + gaps;
            float       x      = inner_x + std::max(0.0F, (inner_w - width) / 2);
            const float height = sizes.player_height * scale;
            for (const RowPane &pane : row)
            {
                const float pane_width = (pane.mixer ? middle_width : sizes.player_width) * scale;
                const Rect  area{x, top, pane_width, height};
                if (!pane.mixer)
                {
                    result.players[pane.player] = area;
                }
                else if (!beats_column)
                {
                    result.mixer = area;
                }
                else if (!content.mixer)
                {
                    result.beats = area;
                }
                else
                {
                    // The crossfader in its own height at the bottom, flush with the players; the beat waveforms
                    // above it take the rest of a player's height
                    const float mixer_height = std::min(sizes.mixer_height * scale, height);
                    result.mixer             = Rect{x, top + height - mixer_height, pane_width, mixer_height};
                    result.beats = Rect{x, top, pane_width, std::max(0.0F, height - mixer_height - mdi_gap)};
                }
                x += pane_width + mdi_gap;
            }
            bottom_top = top + height + mdi_gap;
        }

        if (has_lists)
        {
            const float height = std::max(0.0F, window.y + window.height - mdi_gap - bottom_top);
            const float width  = inner_w;
            if (content.library && content.playlist)
            {
                const float list_width =
                    std::min(sizes.playlist_width, std::max(0.0F, width - mdi_gap) * playlist_share);
                const float library_width = std::max(0.0F, width - mdi_gap - list_width);
                result.library            = Rect{inner_x, bottom_top, library_width, height};
                result.playlist           = Rect{inner_x + library_width + mdi_gap, bottom_top, list_width, height};
            }
            else
            {
                (content.library ? result.library : result.playlist) = Rect{inner_x, bottom_top, width, height};
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

    namespace
    {
        /// Of the candidates, the one nearest to value within mdi_snap_distance; std::nullopt if none is that near
        std::optional<float> nearest(float value, std::initializer_list<float> fixed, const std::vector<float> &more)
        {
            std::optional<float> best;
            const auto           consider = [&](float candidate)
            {
                const float distance = std::abs(candidate - value);
                if (distance <= mdi_snap_distance && (!best || distance < std::abs(*best - value)))
                {
                    best = candidate;
                }
            };
            for (const float candidate : fixed)
            {
                consider(candidate);
            }
            for (const float candidate : more)
            {
                consider(candidate);
            }
            return best;
        }

        /// Where the start of a pane of length size may go so that one of its edges meets an edge of another: its
        /// start at their start or end, or its end at their start or end
        std::vector<float> meetingStarts(const std::vector<Rect> &others, float size, bool across)
        {
            std::vector<float> starts;
            for (const Rect &other : others)
            {
                const float start = across ? other.x : other.y;
                const float end   = start + (across ? other.width : other.height);
                starts.insert(starts.end(), {start, end, start - size, end - size});
            }
            return starts;
        }

        /// Where the end of a pane may go to meet an edge of another
        std::vector<float> meetingEnds(const std::vector<Rect> &others, bool across)
        {
            std::vector<float> ends;
            for (const Rect &other : others)
            {
                const float start = across ? other.x : other.y;
                ends.insert(ends.end(), {start, start + (across ? other.width : other.height)});
            }
            return ends;
        }
    } // namespace

    Rect snapMove(const Rect &moved, const Rect &window, float grid, const std::vector<Rect> &others)
    {
        const Rect inside  = keepInside(moved, window);
        Rect       snapped = inside;
        snapped.x          = onGrid(snapped.x, window.x, grid);
        snapped.y          = onGrid(snapped.y, window.y, grid);
        // Rounding may have pushed it over the right or bottom edge: then the grid line before
        if (snapped.x + snapped.width > window.x + window.width)
        {
            snapped.x = std::max(window.x, gridBefore(window.x + window.width - snapped.width, window.x, grid));
        }
        if (snapped.y + snapped.height > window.y + window.height)
        {
            snapped.y = std::max(window.y, gridBefore(window.y + window.height - snapped.height, window.y, grid));
        }
        // An edge near the window's or another pane's edge goes there instead
        if (const auto x = nearest(inside.x, {window.x, window.x + window.width - inside.width},
                                   meetingStarts(others, inside.width, true)))
        {
            snapped.x = *x;
        }
        if (const auto y = nearest(inside.y, {window.y, window.y + window.height - inside.height},
                                   meetingStarts(others, inside.height, false)))
        {
            snapped.y = *y;
        }
        return keepInside(snapped, window);
    }

    Rect snapResize(const Rect &resized, const Rect &window, float grid, const std::vector<Rect> &others)
    {
        Rect        snapped = resized;
        const float edge_x  = resized.x + resized.width;
        const float edge_y  = resized.y + resized.height;
        const float right   = std::min(nearest(edge_x, {window.x + window.width}, meetingEnds(others, true))
                                           .value_or(onGrid(edge_x, window.x, grid)),
                                       window.x + window.width);
        const float bottom  = std::min(nearest(edge_y, {window.y + window.height}, meetingEnds(others, false))
                                           .value_or(onGrid(edge_y, window.y, grid)),
                                       window.y + window.height);
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
