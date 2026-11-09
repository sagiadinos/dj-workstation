// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_PLAY_BUTTON_HPP
#define DJW_PLAYER_UI_SDL_PLAY_BUTTON_HPP

#include "widget.hpp"

namespace djw::player_ui::sdl
{
    /**
     * Play/pause button. Triggers on press, not on release: a DJ expects the track to start the moment the finger
     * hits the button. Shows the pause symbol while the deck plays.
     */
    class PlayButton final : public Widget
    {
        public:
            /// True once per press; the caller decides what the press means
            bool takePress() noexcept;

            void setPlaying(bool playing) noexcept { showing_pause = playing; }

            void pointerDown(Point position) override;
            void pointerMove(Point position) override;
            void pointerUp(Point position) override;

            void draw(Canvas &canvas) const override;

        private:
            bool pressed       = false;
            bool press_pending = false;
            bool showing_pause = false;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_PLAY_BUTTON_HPP
