// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_LAYOUT_HPP
#define DJW_PLAYER_UI_SDL_LAYOUT_HPP

#include "api/crossfader_side.hpp"

#include <cstdint>
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

    /// What a player shows and does, kept with the layout
    struct PlayerOptions
    {
            /// The display shows the remaining time instead of the elapsed one
            bool show_remaining = false;
            /// A track loaded into the player starts at its cue point (Cue-Load)
            bool cue_load = false;

            bool operator==(const PlayerOptions &) const = default;
    };

    struct PlayerPlacement
    {
            Placement place;
            /// The side of the crossfader the player is on
            core::api::CrossfaderSide side = core::api::CrossfaderSide::None;
            PlayerOptions             options{};

            bool operator==(const PlayerPlacement &) const = default;
    };

    /// How the program shows its parts: each in a window of its own, or all in one window (MDI)
    enum class WindowMode : std::uint8_t
    {
        Separate,
        Mdi
    };

    /**
     * Where the parts of the one window (the MDI mode) are when the user arranged them by hand, in points of a window
     * of the size area; in a window of another size they are scaled with it. A part without a place gets a free one
     * when it opens (a player added later, say). The crossfader and the playlist keep their place while they are
     * closed.
     */
    struct MdiPlaces
    {
            /// The window size the places are for (x and y unused)
            Placement                             area;
            std::vector<std::optional<Placement>> players;
            std::optional<Placement>              mixer;
            std::optional<Placement>              playlist;
            std::optional<Placement>              beats;
            Placement                             library;

            bool operator==(const MdiPlaces &) const = default;
    };

    /**
     * What one window (the MDI mode) shows: the players with their crossfader sides and whether the crossfader, the
     * playlist and the beat waveforms are open; the library is always there. Without places the parts are arranged
     * automatically.
     */
    struct MdiLayout
    {
            std::vector<core::api::CrossfaderSide> players;
            /// The options of each player, as many as players
            std::vector<PlayerOptions> player_options;
            bool                       mixer_open    = true;
            bool                       playlist_open = false;
            bool                       beats_open    = false;
            /// The one window covers the display; otherwise it is a window with a title bar
            bool fullscreen = true;
            /// Not in fullscreen (and after it): maximized, else a window of window_width x window_height (0 x 0:
            /// the default size, see SdlApp)
            bool maximized     = true;
            int  window_width  = 0;
            int  window_height = 0;
            /// std::nullopt: arranged automatically
            std::optional<MdiPlaces> places;

            bool operator==(const MdiLayout &) const = default;
    };

    /// What the MDI mode shows without a saved layout: two players on the sides A and B, the crossfader open, in a
    /// window of the default size
    [[nodiscard]] MdiLayout defaultMdiLayout();

    struct ToolPlacement
    {
            bool      open = false;
            Placement place;

            bool operator==(const ToolPlacement &) const = default;
    };

    /**
     * The windows of the program as the user left them: how many players there are, where each one is and on which
     * crossfader side, the crossfader window, the playlist window and the beat waveforms (open or not, where), and the
     * library window. Saved by the View menu and read at the next start. The beat waveforms came later: a layout
     * without them has them closed.
     *
     * Each mode keeps its own layout: the fields above mdi are the separate windows, mdi is the one window. mode is
     * the mode the next start uses.
     */
    struct Layout
    {
            std::vector<PlayerPlacement> players;
            Placement                    library;
            ToolPlacement                mixer;
            ToolPlacement                playlist;
            ToolPlacement                beats{.open = false, .place = {0, 0, 760, 200}};
            /// The beat waveforms in vertical lanes, in both modes
            bool       beats_vertical = false;
            WindowMode mode           = WindowMode::Separate;
            /// std::nullopt: the MDI mode was not saved yet, it starts with defaultMdiLayout()
            std::optional<MdiLayout> mdi;

            bool operator==(const Layout &) const = default;
    };

    /// The most players a layout may have; a file with more is not a layout
    inline constexpr std::size_t max_layout_players = 32;

    /// The layout as text, one window per line (see parseLayout)
    [[nodiscard]] std::string formatLayout(const Layout &layout);

    /// The layout from text written by formatLayout; std::nullopt for anything else (other version, damaged,
    /// sizes that make no sense), so a broken file falls back to the default layout instead of opening odd windows.
    /// A layout of version 1 (before the MDI mode) is read as the separate windows.
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
