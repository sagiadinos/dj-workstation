// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_LOOK_HPP
#define DJW_PLAYER_UI_SDL_LOOK_HPP

#include "canvas.hpp"

#include <cstdint>

namespace djw::player_ui::sdl::look
{
    /**
     * The colors that differ between the dark look (brushed metal, light text) and the daylight look (light surfaces,
     * dark text, strong contrast for sun or bright rooms). What signals something stays the same in both: play green,
     * CUE red, the meters, and the black displays with what they show (time, waveforms), which read well in daylight
     * too.
     */
    struct Palette
    {
            Color window;    ///< behind everything
            Color metal_top; ///< the background of a window, top to bottom
            Color metal_bottom;
            Color frame;   ///< around a display or field
            Color outline; ///< around buttons, faders, grooves
            Color text;
            Color text_dim;    ///< hints, shortcuts, what is off
            Color label;       ///< names next to controls
            Color status;      ///< the status line of library and playlist
            Color button_gray; ///< the body of a plain button
            Color button_blue; ///< the body of a button that is on
            Color disabled_background;
            Color disabled_text;
            Color field;     ///< the inside of a check box or stepper
            Color field_top; ///< the inside of a text field, top to bottom
            Color field_bottom;
            Color field_frame;
            Color groove_top; ///< the groove of a fader, top to bottom
            Color groove_bottom;
            Color row; ///< list rows, one after the other
            Color row_alternate;
            Color secondary_text; ///< columns other than artist and title
            Color missing_text;   ///< a track whose file is gone
            Color highlight_top;  ///< the entry under the pointer or chosen in a menu or list, top to bottom
            Color highlight_bottom;
            Color highlight_text;
            Color menu_bar_top; ///< the menu bar, top to bottom
            Color menu_bar_bottom;
            Color menu_box_top; ///< an open menu, top to bottom
            Color menu_box_bottom;
            Color item_top; ///< an entry of an open drop-down list, top to bottom
            Color item_bottom;
            Color separator;
            Color header;      ///< the column header of a list
            Color pane_border; ///< around a part of the one window
            Color pane_bar;    ///< its title bar
            Color focus;       ///< the line around the part that has the keyboard
            Color bubble;      ///< a help bubble
    };

    /// The dark look, the default
    [[nodiscard]] const Palette &darkPalette() noexcept;
    /// The daylight look
    [[nodiscard]] const Palette &daylightPalette() noexcept;
    /// The look in use; every widget asks for its colors when it draws, so a switch shows on the next frame
    [[nodiscard]] const Palette &colors() noexcept;
    /// Switches between the dark look and the daylight look (View > Daylight colors)
    void               setDaylight(bool on) noexcept;
    [[nodiscard]] bool daylight() noexcept;

    // Colors the same in both looks: yellow on black for what the display shows
    inline constexpr Color display_black{0, 0, 0};
    inline constexpr Color display_yellow{255, 218, 0};
    inline constexpr Color accent_blue{20, 130, 235};

    /// A color moved towards white (amount > 0) or black (amount < 0), in steps of 0 to 255
    [[nodiscard]] Color shade(Color color, int amount) noexcept;

    /// The brushed metal of a window
    void background(Canvas &canvas, const Rect &area);

    /// A button body: rounded, glossy gradient around base, a dark outline; pressed it is darker and reversed
    void raised(Canvas &canvas, const Rect &area, Color base, bool pressed, float radius = 8.0F);

    /// A recessed black panel with a thin frame, for displays and text fields
    void sunken(Canvas &canvas, const Rect &area, Color fill = display_black, float radius = 6.0F);

    /// The groove of a fader or slider; level is the part filled from the start (0 to 1, < 0: no fill)
    void groove(Canvas &canvas, const Rect &area, bool vertical, float level, Color fill = accent_blue);

    /// The grip of a fader or slider; mark_horizontal: the line across the grip runs left to right. With alpha below
    /// 255 it lets what lies under it show through (a level meter behind a fader).
    void knob(Canvas &canvas, const Rect &area, bool held, bool mark_horizontal, std::uint8_t alpha = 255);

    /// The quietest level a meter shows, in dB below full scale
    inline constexpr float meter_floor_db = -40.0F;
    /// Where green turns yellow and yellow turns red on a meter, in dBFS
    inline constexpr float meter_yellow_db = -12.0F;
    inline constexpr float meter_red_db    = -3.0F;
    /// Where a peak level (linear, 1.0 = 0 dBFS) lies on a meter: 0 at meter_floor_db and below, 1 at 0 dBFS and above
    [[nodiscard]] float meterPosition(float linear) noexcept;
    /**
     * A stereo LED meter: two lanes of segments (left, right), green, from meter_yellow_db yellow, from meter_red_db
     * red, unlit segments dark. vertical: they rise from the bottom; otherwise they run from the left. The clip lamp
     * takes the far end (top or right) and is lit while clip.
     */
    void meter(Canvas &canvas, const Rect &area, float left, float right, bool vertical, bool clip);

    /// A filter funnel with a small cross at its lower right: clear the filters
    void clearFilterIcon(Canvas &canvas, const Rect &area, Color color);
    /// Two lanes, one above the other (horizontal) or side by side (vertical): how the beat waveforms stand
    void lanesIcon(Canvas &canvas, const Rect &area, bool vertical, Color color);
    /// Headphones: a headband over two ear cups
    void headphonesIcon(Canvas &canvas, const Rect &area, Color color);
    /// A round amber LED, glowing when lit and dark otherwise, as on a mixer
    void led(Canvas &canvas, const Rect &area, bool lit);
    /// A check mark, for a menu entry that is on
    void checkMark(Canvas &canvas, const Rect &area, Color color);
} // namespace djw::player_ui::sdl::look

#endif // DJW_PLAYER_UI_SDL_LOOK_HPP
