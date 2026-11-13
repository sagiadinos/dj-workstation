// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_CANVAS_HPP
#define DJW_PLAYER_UI_SDL_CANVAS_HPP

#include "geometry.hpp"
#include "picture.hpp"

#include <cstdint>
#include <optional>
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
            /// A rectangle with rounded corners, filled with a vertical gradient from top to bottom (the same color
            /// twice is flat). A canvas that cannot do this draws a plain rectangle in the top color.
            virtual void fillRoundedRect(const Rect &rect, float /*radius*/, Color top, Color /*bottom*/)
            {
                fillRect(rect, top);
            }
            /// One line of UTF-8 text, position is its top left corner; the line is textHeight() high
            virtual void text(Point position, std::string_view text, Color color) = 0;
            /// Like text(), but cut with "…" if it is wider than max_width
            virtual void textElided(Point position, std::string_view text, Color color, float max_width) = 0;
            [[nodiscard]] virtual float textHeight() const noexcept                                      = 0;
            [[nodiscard]] virtual float textWidth(std::string_view text) const noexcept                  = 0;

            /// Like text(), in the large size of the time display
            virtual void                largeText(Point position, std::string_view text, Color color) = 0;
            [[nodiscard]] virtual float largeTextHeight() const noexcept                              = 0;
            [[nodiscard]] virtual float largeTextWidth(std::string_view text) const noexcept          = 0;

            /// Makes all text (normal and large) this many times as big, until set again; 1 is the normal size. The
            /// sizes textHeight(), textWidth() and so on then report are those of the scaled text. A canvas that cannot
            /// scale text ignores it.
            virtual void setTextScale(float /*scale*/) {}

            /// The picture stretched over area (its aspect is the caller's business, see aspectOf()), alpha 255 opaque.
            /// A canvas that cannot draw pictures leaves them out.
            virtual void picture(Picture /*which*/, const Rect & /*area*/, std::uint8_t /*alpha*/) {}

            /// Drawing outside clip is cut off until the next setClip(); std::nullopt draws everywhere again
            virtual void setClip(std::optional<Rect> clip) = 0;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_CANVAS_HPP
