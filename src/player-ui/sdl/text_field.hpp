// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_TEXT_FIELD_HPP
#define DJW_PLAYER_UI_SDL_TEXT_FIELD_HPP

#include "keyboard_input.hpp"
#include "widget.hpp"

#include <string>

namespace djw::player_ui::sdl
{
    /**
     * One-line text input. A tap focuses it; while focused it takes typed text, Backspace removes the last
     * character, Escape clears it. The text is UTF-8; editing happens at the end.
     */
    class TextField final : public Widget
    {
        public:
            explicit TextField(std::string placeholder = {}) : placeholder{std::move(placeholder)} {}

            [[nodiscard]] const std::string &text() const noexcept { return content; }
            [[nodiscard]] bool               focused() const noexcept { return has_focus; }
            void                             setFocused(bool focused) noexcept { has_focus = focused; }

            /// Applies the input if focused; true when the text changed
            bool input(const KeyboardInput &input);

            void pointerDown(Point position) override;
            void pointerMove(Point /*position*/) override {}
            void pointerUp(Point /*position*/) override {}

            void draw(Canvas &canvas) const override;

        private:
            std::string placeholder;
            std::string content;
            bool        has_focus = false;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_TEXT_FIELD_HPP
