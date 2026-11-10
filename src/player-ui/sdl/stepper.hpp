// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_STEPPER_HPP
#define DJW_PLAYER_UI_SDL_STEPPER_HPP

#include "widget.hpp"

#include <utility>

namespace djw::player_ui::sdl
{
    /// A whole number with a minus and a plus button on its sides, e.g. the fade time in seconds
    class Stepper final : public Widget
    {
        public:
            Stepper(int minimum, int maximum, int value) noexcept : low{minimum}, high{maximum}, current{value} {}

            [[nodiscard]] int value() const noexcept { return current; }
            void              setValue(int value) noexcept;
            /// True once after the user changed the value (not after setValue)
            [[nodiscard]] bool takeChanged() noexcept { return std::exchange(changed, false); }

            void pointerDown(Point position) override;
            void pointerMove(Point /*position*/) override {}
            void pointerUp(Point position) override;

            void draw(Canvas &canvas) const override;

        private:
            enum class Part : unsigned char
            {
                None,
                Minus,
                Plus
            };

            [[nodiscard]] Part partAt(Point position) const noexcept;

            int  low;
            int  high;
            int  current;
            Part pressed = Part::None;
            bool changed = false;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_STEPPER_HPP
