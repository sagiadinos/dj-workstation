// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_TEXT_FIELD_HPP
#define DJW_PLAYER_UI_SDL_TEXT_FIELD_HPP

#include "keyboard_input.hpp"
#include "widget.hpp"

#include <string>
#include <utility>

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
            void                             setText(std::string text) { content = std::move(text); }
            /// The hint shown while the field is empty
            void               setPlaceholder(std::string hint) { placeholder = std::move(hint); }
            [[nodiscard]] bool focused() const noexcept { return has_focus; }
            void                             setFocused(bool focused) noexcept { has_focus = focused; }
            /// True once after a tap focused the field, so the view can take the focus from the other fields
            [[nodiscard]] bool takeFocusRequest() noexcept { return std::exchange(focus_requested, false); }

            /// Applies the input if focused; true when the text changed
            bool input(const KeyboardInput &input);

            void pointerDown(Point position) override;
            void pointerMove(Point /*position*/) override {}
            void pointerUp(Point /*position*/) override {}

            void draw(Canvas &canvas) const override;

        private:
            std::string placeholder;
            std::string content;
            bool        has_focus       = false;
            bool        focus_requested = false;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_TEXT_FIELD_HPP
