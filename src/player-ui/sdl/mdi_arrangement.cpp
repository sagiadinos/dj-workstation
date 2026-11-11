// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "mdi_arrangement.hpp"

#include <algorithm>

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
} // namespace djw::player_ui::sdl
