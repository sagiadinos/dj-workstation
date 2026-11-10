// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_TEXT_BUTTON_HPP
#define DJW_PLAYER_UI_SDL_TEXT_BUTTON_HPP

#include "widget.hpp"

#include <optional>
#include <string>
#include <utility>

namespace djw::player_ui::sdl
{
    /**
     * A button with a text label. A press counts when the pointer goes down and up on the button, so sliding off it
     * cancels. With empty bounds it is hidden: it neither draws nor catches pointers.
     *
     * For buttons that act while held (CUE, pitch bend, momentary A/B) there are the edges: takeDown() once when the
     * pointer touched it, takeUp() once when it let go, wherever that was. A disabled button is drawn dimmed and
     * takes no input.
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

            /// A disabled button is dimmed and ignores the pointer
            void               setEnabled(bool on) noexcept;
            [[nodiscard]] bool isEnabled() const noexcept { return enabled; }

            /// Own colors instead of the standard ones, e.g. the red CUE button; std::nullopt for the standard
            void setColors(std::optional<Color> background, std::optional<Color> label_color) noexcept
            {
                custom_background = background;
                custom_text       = label_color;
            }

            /// True once after a press
            [[nodiscard]] bool takePress() noexcept { return std::exchange(pressed, false); }
            /// True once after the pointer touched the button
            [[nodiscard]] bool takeDown() noexcept { return std::exchange(down_edge, false); }
            /// True once after the pointer that touched the button let go
            [[nodiscard]] bool takeUp() noexcept { return std::exchange(up_edge, false); }
            /// The pointer is on the button now
            [[nodiscard]] bool held() const noexcept { return down; }

            void pointerDown(Point /*position*/) override;
            void pointerMove(Point /*position*/) override {}
            void pointerUp(Point position) override;

            void draw(Canvas &canvas) const override;

        private:
            std::string          text;
            std::optional<Color> custom_background;
            std::optional<Color> custom_text;
            bool                 highlighted = false;
            bool                 enabled     = true;
            bool                 down        = false;
            bool                 pressed     = false;
            bool                 down_edge   = false;
            bool                 up_edge     = false;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_TEXT_BUTTON_HPP
