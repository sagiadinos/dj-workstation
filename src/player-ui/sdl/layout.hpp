// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_LAYOUT_HPP
#define DJW_PLAYER_UI_SDL_LAYOUT_HPP

#include "api/crossfader_side.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace djw::player_ui::sdl
{
    /// Position and size of a window on the desktop, in points
    struct Placement
    {
            int x      = 0;
            int y      = 0;
            int width  = 0;
            int height = 0;

            bool operator==(const Placement &) const = default;
    };

    struct PlayerPlacement
    {
            Placement                 place;
            /// The side of the crossfader the player is on
            core::api::CrossfaderSide side = core::api::CrossfaderSide::None;

            bool operator==(const PlayerPlacement &) const = default;
    };

    struct ToolPlacement
    {
            bool      open = false;
            Placement place;

            bool operator==(const ToolPlacement &) const = default;
    };

    /**
     * The windows of the program as the user left them: how many players there are, where each one is and on which
     * crossfader side, the crossfader window and the playlist window (open or not, where), and the library window.
     * Saved by the View menu and read at the next start.
     */
    struct Layout
    {
            std::vector<PlayerPlacement> players;
            Placement                    library;
            ToolPlacement                mixer;
            ToolPlacement                playlist;

            bool operator==(const Layout &) const = default;
    };

    /// The most players a layout may have; a file with more is not a layout
    inline constexpr std::size_t max_layout_players = 32;

    /// The layout as text, one window per line (see parseLayout)
    [[nodiscard]] std::string formatLayout(const Layout &layout);

    /// The layout from text written by formatLayout; std::nullopt for anything else (other version, damaged,
    /// sizes that make no sense), so a broken file falls back to the default layout instead of opening odd windows
    [[nodiscard]] std::optional<Layout> parseLayout(std::string_view text);

    /// The sizes the default layout is made of
    struct DefaultSizes
    {
            int player_width;
            int player_height;
            int mixer_width;
            int mixer_height;
            int library_width;
            int library_height;
    };

    /**
     * What the program shows without a saved layout: two players with the crossfader window between them (player 1
     * on side A, player 2 on side B), side by side at the top of the work area and centered, the library window below.
     * When the windows do not fit next to each other they are moved inside the work area as far as possible.
     */
    [[nodiscard]] Layout defaultLayout(const Placement &work_area, const DefaultSizes &sizes);

    /// The placement moved (and, if it is bigger than the area, shrunk) so that it lies inside the area
    [[nodiscard]] Placement fitInto(Placement place, const Placement &area);
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_LAYOUT_HPP
