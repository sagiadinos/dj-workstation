// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "mdi_arrangement.hpp"

#include <algorithm>
#include <cmath>

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
} // namespace djw::player_ui::sdl
