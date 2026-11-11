// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_PANE_FRAME_HPP
#define DJW_PLAYER_UI_SDL_PANE_FRAME_HPP

#include "text_button.hpp"
#include "view.hpp"

#include <functional>
#include <string>

namespace djw::player_ui::sdl
{
    /**
     * A view inside the one window of the MDI mode: a title bar on top, with a close button if there is a close
     * action, and the view below it, all within a light line of 1 point, so neighbouring parts can be told apart.
     * Everything but the close button goes to the view. The title is asked for on every draw, so it follows the
     * language.
     *
     * close is called from update(), while the window is being drawn: it should only ask for the pane to go, which
     * then happens between frames.
     */
    class PaneFrame final : public View
    {
        public:
            using Title = std::function<std::string()>;
            using Close = std::function<void()>;

            static constexpr float title_height = 30.0F;

            /// The content must outlive the frame
            PaneFrame(View &content, Title title, Close close = {});

            void layout(const Rect &area) override;
            void addTo(PointerRouter &router) override;
            void update() override;
            void draw(Canvas &canvas) const override;

            [[nodiscard]] bool wantsTextInput() const override { return content->wantsTextInput(); }
            void               keyboard(const KeyboardInput &input) override { content->keyboard(input); }
            void               wheel(Point position, float dy) override { content->wheel(position, dy); }
            void               secondaryClick(Point position) override { content->secondaryClick(position); }

            [[nodiscard]] View             &inner() const noexcept { return *content; }
            [[nodiscard]] const TextButton &closeButton() const noexcept { return close_button; }
            /// The title bar
            [[nodiscard]] const Rect &bar() const noexcept { return title_bar; }
            /// Width of the line around the part
            static constexpr float border = 1.0F;
            static constexpr Color border_color{110, 112, 120};

        private:
            View      *content;
            Title      title;
            Close      close;
            Rect       title_bar;
            Rect       whole; ///< the part with its border
            TextButton close_button{"×"};
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_PANE_FRAME_HPP
