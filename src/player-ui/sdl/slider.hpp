// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_SLIDER_HPP
#define DJW_PLAYER_UI_SDL_SLIDER_HPP

#include "widget.hpp"

#include <utility>

namespace djw::player_ui::sdl
{
    /**
     * Horizontal slider with a value from 0.0 (left) to 1.0 (right), for the position in a track and the crossfader.
     *
     * Like the Fader the value follows the finger: touching sets it to that position, dragging moves it. The owner
     * sets the value from the state it shows, except while held() (the finger is the truth then) and reads
     * takeChanged() to learn that the user moved it.
     */
    class Slider final : public Widget
    {
        public:
            [[nodiscard]] float value() const noexcept { return current; }
            void                setValue(float value) noexcept;
            [[nodiscard]] bool  held() const noexcept { return is_held; }
            /// True once after the user moved the slider (not after setValue)
            [[nodiscard]] bool takeChanged() noexcept { return std::exchange(changed, false); }

            /// Colors of the slider's knob side; the track stays gray
            void setAccent(Color color) noexcept { accent = color; }
            /// Fill the groove up to the grip (default: no fill, the groove stays dark)
            void setShowFill(bool on) noexcept { show_fill = on; }

            void pointerDown(Point position) override;
            void pointerMove(Point position) override;
            void pointerUp(Point position) override;

            void draw(Canvas &canvas) const override;

            static constexpr float knob_width = 18.0F;

        private:
            /// The part of the bounds the value maps to (the knob must stay inside the bounds)
            [[nodiscard]] Rect track() const noexcept;
            void               follow(Point position) noexcept;

            float current = 0.0F;
            bool  is_held = false;
            bool  changed = false;
            Color accent{0, 150, 200};
            bool  show_fill = false;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_SLIDER_HPP
