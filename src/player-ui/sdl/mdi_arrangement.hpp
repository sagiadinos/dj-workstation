// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_MDI_ARRANGEMENT_HPP
#define DJW_PLAYER_UI_SDL_MDI_ARRANGEMENT_HPP

#include "geometry.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace djw::player_ui::sdl
{
    /// What the one window of the MDI mode shows
    struct MdiContent
    {
            std::size_t players  = 0;
            bool        mixer    = false;
            bool        playlist = false;
            bool        library  = false;
    };

    /// The sizes the panes are made for, title bar included
    struct MdiSizes
    {
            float player_width   = 0.0F;
            float player_height  = 0.0F;
            float mixer_width    = 0.0F;
            float playlist_width = 0.0F;
    };

    /// Where each pane goes; a pane that is not shown has no area
    struct MdiArrangement
    {
            std::vector<Rect>   players;
            std::optional<Rect> mixer;
            std::optional<Rect> playlist;
            std::optional<Rect> library;
    };

    /// Space around and between the panes
    inline constexpr float mdi_gap = 6.0F;
    /// The most of the window the row of players takes when the library is below it
    inline constexpr float mdi_row_share = 0.6F;

    /**
     * The panes laid out in the window, none overlapping: the players in a row at the top with the crossfader between
     * player 1 and player 2, centered; the library below over the whole width with the playlist to its right. The row
     * keeps the proportions of the players and shrinks as a whole when it does not fit, never grows beyond the sizes
     * it is made for.
     */
    [[nodiscard]] MdiArrangement arrangeMdi(const Rect &window, const MdiContent &content, const MdiSizes &sizes);
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_MDI_ARRANGEMENT_HPP
