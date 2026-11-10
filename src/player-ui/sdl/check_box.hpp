// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_CHECK_BOX_HPP
#define DJW_PLAYER_UI_SDL_CHECK_BOX_HPP

#include "widget.hpp"

#include <string>
#include <utility>

namespace djw::player_ui::sdl
{
    /// A box with a label that is on or off; a tap on the box or its label (down and up on it) toggles it
    class CheckBox final : public Widget
    {
        public:
            explicit CheckBox(std::string label = {}) : text{std::move(label)} {}

            void                             setLabel(std::string label) { text = std::move(label); }
            [[nodiscard]] const std::string &label() const noexcept { return text; }

            [[nodiscard]] bool checked() const noexcept { return on; }
            void               setChecked(bool checked) noexcept { on = checked; }
            /// True once after the user toggled it
            [[nodiscard]] bool takeToggled() noexcept { return std::exchange(toggled, false); }

            void pointerDown(Point /*position*/) override { down = true; }
            void pointerMove(Point /*position*/) override {}
            void pointerUp(Point position) override;

            void draw(Canvas &canvas) const override;

        private:
            std::string text;
            bool        on      = false;
            bool        down    = false;
            bool        toggled = false;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_CHECK_BOX_HPP
