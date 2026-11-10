// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_FADER_HPP
#define DJW_PLAYER_UI_SDL_FADER_HPP

#include "widget.hpp"

namespace djw::player_ui::sdl
{
    /**
     * Vertical fader with a value from 0.0 (bottom) to 1.0 (top).
     *
     * The value follows the finger directly: touching the fader sets it to that position, dragging moves it,
     * outside the track it stops at the end.
     */
    class Fader final : public Widget
    {
        public:
            [[nodiscard]] float value() const noexcept { return current; }
            void                setValue(float value) noexcept;

            void pointerDown(Point position) override;
            void pointerMove(Point position) override;
            void pointerUp(Point position) override;

            void draw(Canvas &canvas) const override;

        private:
            /// The part of the bounds the value maps to (the knob must stay inside the bounds)
            [[nodiscard]] Rect track() const noexcept;
            void               follow(Point position) noexcept;

            float current = 1.0F;
            bool  held    = false;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_FADER_HPP
