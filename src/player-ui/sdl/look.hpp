// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_LOOK_HPP
#define DJW_PLAYER_UI_SDL_LOOK_HPP

#include "canvas.hpp"

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

    /// The grip of a fader or slider; mark_horizontal: the line across the grip runs left to right
    void knob(Canvas &canvas, const Rect &area, bool held, bool mark_horizontal);

    /// An open folder, for the Load button
    void folderIcon(Canvas &canvas, const Rect &area, Color color);
} // namespace djw::player_ui::sdl::look

#endif // DJW_PLAYER_UI_SDL_LOOK_HPP
