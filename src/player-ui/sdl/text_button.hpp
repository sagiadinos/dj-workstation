// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_TEXT_BUTTON_HPP
#define DJW_PLAYER_UI_SDL_TEXT_BUTTON_HPP

#include "widget.hpp"

#include <string>
#include <utility>

namespace djw::player_ui::sdl
{
    /**
     * A button with a text label. A press counts when the pointer goes down and up on the button, so sliding off it
     * cancels. With empty bounds it is hidden: it neither draws nor catches pointers.
     */
    class TextButton final : public Widget
    {
        public:
            explicit TextButton(std::string label = {}) : text{std::move(label)} {}

            void                             setLabel(std::string label) { text = std::move(label); }
            [[nodiscard]] const std::string &label() const noexcept { return text; }

            /// Drawn in the accent color, e.g. for an option that is on
            void               setHighlighted(bool on) noexcept { highlighted = on; }
            [[nodiscard]] bool isHighlighted() const noexcept { return highlighted; }

            /// True once after a press
            [[nodiscard]] bool takePress() noexcept { return std::exchange(pressed, false); }

            void pointerDown(Point /*position*/) override { down = true; }
            void pointerMove(Point /*position*/) override {}
            void pointerUp(Point position) override;

            void draw(Canvas &canvas) const override;

        private:
            std::string text;
            bool        highlighted = false;
            bool        down        = false;
            bool        pressed     = false;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_TEXT_BUTTON_HPP
