// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_GEOMETRY_HPP
#define DJW_PLAYER_UI_SDL_GEOMETRY_HPP

#include <algorithm>

namespace djw::player_ui::sdl
{
    /// Position in window coordinates (points, not pixels)
    struct Point
    {
            float x = 0.0F;
            float y = 0.0F;
    };

    struct Rect
    {
            float x      = 0.0F;
            float y      = 0.0F;
            float width  = 0.0F;
            float height = 0.0F;

            bool contains(Point point) const noexcept
            {
                return point.x >= x && point.x < x + width && point.y >= y && point.y < y + height;
            }

            /// The rectangle shrunk by margin on every side, never negative in size
            Rect inset(float margin) const noexcept
            {
                return {x + margin, y + margin, std::max(0.0F, width - 2 * margin),
                        std::max(0.0F, height - 2 * margin)};
            }
    };

    struct Color
    {
            unsigned char red   = 0;
            unsigned char green = 0;
            unsigned char blue  = 0;
            unsigned char alpha = 255;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_GEOMETRY_HPP
