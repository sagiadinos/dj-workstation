// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_CANVAS_HPP
#define DJW_PLAYER_UI_SDL_CANVAS_HPP

#include "geometry.hpp"

#include <string_view>

namespace djw::player_ui::sdl
{
    /// What widgets draw on. Keeps the widgets free of SDL, so they can be tested without a window.
    class Canvas
    {
        public:
            Canvas()                          = default;
            Canvas(const Canvas &)            = delete;
            Canvas &operator=(const Canvas &) = delete;
            Canvas(Canvas &&)                 = delete;
            Canvas &operator=(Canvas &&)      = delete;
            virtual ~Canvas()                 = default;

            virtual void fillRect(const Rect &rect, Color color)                           = 0;
            virtual void fillTriangle(Point first, Point second, Point third, Color color) = 0;
            /// Simple text; character height is textHeight()
            virtual void                text(Point position, std::string_view text, Color color) = 0;
            [[nodiscard]] virtual float textHeight() const noexcept                              = 0;
            [[nodiscard]] virtual float textWidth(std::string_view text) const noexcept          = 0;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_CANVAS_HPP
