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

    /// The grid the panes snap to when they are moved or resized by hand, in points from the window's corner
    inline constexpr float mdi_grid = 10.0F;
    /// The smallest a pane can be made by hand
    inline constexpr float mdi_min_width  = 120.0F;
    inline constexpr float mdi_min_height = 100.0F;

    /// Whether two areas share more than an edge
    [[nodiscard]] bool overlap(const Rect &first, const Rect &second) noexcept;

    /// The area kept inside the window: moved in, and shrunk if it is bigger than the window
    [[nodiscard]] Rect keepInside(Rect area, const Rect &window) noexcept;

    /// A pane moved by hand: its corner on the grid, inside the window, the size unchanged
    [[nodiscard]] Rect snapMove(const Rect &moved, const Rect &window, float grid = mdi_grid) noexcept;

    /// A pane resized by hand at its lower right corner: that corner on the grid, at least the smallest size,
    /// inside the window; the upper left corner stays
    [[nodiscard]] Rect snapResize(const Rect &resized, const Rect &window, float grid = mdi_grid) noexcept;

    /**
     * A place in the window for a pane of the size wanted that overlaps none of the taken areas: the first on the grid
     * from the top left, row by row. If there is none, the pane is made smaller step by step down to half its size;
     * std::nullopt if it does not fit even then.
     */
    [[nodiscard]] std::optional<Rect> freePlace(float width, float height, const Rect &window,
                                                const std::vector<Rect> &taken, float grid = mdi_grid);
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_MDI_ARRANGEMENT_HPP
