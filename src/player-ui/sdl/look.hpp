// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_LOOK_HPP
#define DJW_PLAYER_UI_SDL_LOOK_HPP

#include "canvas.hpp"

#include <cstdint>

namespace djw::player_ui::sdl::look
{
    // The palette: dark brushed metal, glossy controls, yellow on black for what the display shows
    inline constexpr Color text{232, 232, 235};
    inline constexpr Color text_dim{120, 122, 128};
    inline constexpr Color display_black{0, 0, 0};
    inline constexpr Color display_yellow{255, 218, 0};
    inline constexpr Color accent_blue{20, 130, 235};
    inline constexpr Color button_gray{68, 70, 75};
    inline constexpr Color button_blue{40, 75, 120};
    inline constexpr Color outline{14, 14, 16};

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

    /// An open folder, for the Load button
    void folderIcon(Canvas &canvas, const Rect &area, Color color);
} // namespace djw::player_ui::sdl::look

#endif // DJW_PLAYER_UI_SDL_LOOK_HPP
