// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_VIEW_HOST_HPP
#define DJW_PLAYER_UI_SDL_VIEW_HOST_HPP

#include "canvas.hpp"
#include "geometry.hpp"
#include "keyboard_input.hpp"
#include "pointer_event.hpp"
#include "pointer_router.hpp"
#include "view.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace djw::player_ui::sdl
{
    /**
     * The views one window (a host) shows, each in its own rectangle of the window. Keeps SDL out, so it can be
     * tested without a window.
     *
     * A view without an area of its own fills the whole window; that is how a window with one view (one window per
     * player, library, mixer, playlist) works. Views with an area lie side by side; they are not meant to overlap.
     *
     * Input: pointer events go to the widgets of all views (they are in window coordinates), the wheel and the right
     * mouse button to the view under the pointer, typed text to the focused view: the one a pointer last went down
     * on, the first view until then.
     */
    class ViewHost
    {
        public:
            /// Adds a view on top of the others; area std::nullopt fills the window. The view must outlive the host
            /// or be removed first. A view that is already there is not added twice.
            void add(View &view, std::optional<Rect> area = std::nullopt);
            /// Removes the view; its widgets get no more input. Unknown views are ignored.
            void remove(const View &view);
            /// Gives the view a new area (std::nullopt: the whole window), used from the next layout()
            void place(const View &view, std::optional<Rect> area);

            [[nodiscard]] std::size_t size() const noexcept { return panes.size(); }
            [[nodiscard]] bool        empty() const noexcept { return panes.empty(); }
            [[nodiscard]] bool        contains(const View &view) const noexcept;
            /// The area the view was laid out in at the last layout(); std::nullopt for an unknown view
            [[nodiscard]] std::optional<Rect> areaOf(const View &view) const noexcept;
            /// The view typed text goes to; nullptr without views
            [[nodiscard]] View *focused() const noexcept;

            /// Lays every view out in its area; window is the whole window
            void layout(const Rect &window);
            void update();
            /// Draws the views in the order they were added
            void draw(Canvas &canvas) const;

            void pointer(const PointerEvent &event);
            void keyboard(const KeyboardInput &input);
            void wheel(Point position, float dy);
            void secondaryClick(Point position);
            /// True while the focused view takes typed text
            [[nodiscard]] bool wantsTextInput() const;

        private:
            struct Pane
            {
                    View               *view = nullptr;
                    std::optional<Rect> area;   ///< as asked for; std::nullopt: the whole window
                    Rect                placed; ///< as laid out
            };

            /// The topmost view whose area holds position; nullptr if none
            [[nodiscard]] View                             *viewAt(Point position) const noexcept;
            [[nodiscard]] std::vector<Pane>::const_iterator find(const View &view) const noexcept;
            /// The widgets of all views, again from scratch: after a view came or went
            void rebuildRouter();

            std::vector<Pane> panes;
            PointerRouter     router;
            View             *focus = nullptr;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_VIEW_HOST_HPP
