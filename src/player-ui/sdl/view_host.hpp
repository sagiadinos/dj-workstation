// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_VIEW_HOST_HPP
#define DJW_PLAYER_UI_SDL_VIEW_HOST_HPP

#include "canvas.hpp"
#include "geometry.hpp"
#include "keyboard_input.hpp"
#include "pointer_event.hpp"
#include "pointer_router.hpp"
#include "text_button.hpp"
#include "view.hpp"

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
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
     * on, the first view until then. Where several views with areas of their own share the window, that one has a
     * blue line around it.
     *
     * Arranging (the layout editing of the MDI mode): the views are locked, their widgets get no input. Instead a
     * view with an area of its own can be dragged anywhere to move it, or at its lower right corner to resize it; it
     * snaps to the grid and stays inside the window. Where it is let go, the owner is told (moved) - unless it would
     * overlap another view, then it goes back. A Done button and Escape end the arranging (done).
     */
    class ViewHost
    {
        public:
            /// Adds a view on top of the others; area std::nullopt fills the window. The view must outlive the host
            /// or be removed first. A view that is already there is not added twice. A view always_on_top stays above
            /// all others, also those added later (the menu bar of the one window, whose menus drop down over them);
            /// it is not arranged by hand.
            void add(View &view, std::optional<Rect> area = std::nullopt, bool always_on_top = false);
            /// The part of the window the views are arranged in by hand (std::nullopt: the whole window), e.g. the
            /// window without the menu bar on top; the grid starts at its corner
            void setWorkArea(std::optional<Rect> area) { work_area = area; }
            /// Removes the view; its widgets get no more input. Unknown views are ignored.
            void remove(const View &view);
            /// Gives the view a new area (std::nullopt: the whole window), used from the next layout()
            void place(const View &view, std::optional<Rect> area);

            [[nodiscard]] std::size_t size() const noexcept { return panes.size(); }
            [[nodiscard]] bool        empty() const noexcept { return panes.empty(); }
            [[nodiscard]] bool        contains(const View &view) const noexcept;
            /// The area the view was laid out in at the last layout(); std::nullopt for an unknown view
            [[nodiscard]] std::optional<Rect> areaOf(const View &view) const noexcept;
            /// Typed text goes to view from now on (e.g. a dialog that opened); ignored for a view not shown
            void setFocus(View &view)
            {
                if (contains(view))
                {
                    focus = &view;
                }
            }
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

            /// What the owner hears while the views are arranged
            struct Arranging
            {
                    /// A view was let go at area (already on the grid, inside the window, overlapping none)
                    std::function<void(const View &view, const Rect &area)> moved;
                    /// Done or Escape: the owner ends the arranging with stopArranging()
                    std::function<void()> done;
                    /// The label of the Done button, asked for on every update so it follows the language
                    std::function<std::string()> done_label;
            };
            /// Width of the line around the view typed keys go to, when several views share the window
            static constexpr float focus_line = 2.0F;
            /// The square at the lower right corner of a view that resizes it
            static constexpr float resize_handle = 32.0F;

            void               startArranging(Arranging arranging);
            void               stopArranging();
            [[nodiscard]] bool arranging() const noexcept { return arrange.has_value(); }
            /// While a view is dragged: where it would go now; std::nullopt otherwise
            [[nodiscard]] std::optional<Rect> dragTarget() const noexcept;
            /// While a view is dragged: whether it could be let go where it is (it overlaps no other view)
            [[nodiscard]] bool              dragFits() const noexcept { return drag && drag->fits; }
            [[nodiscard]] const TextButton &doneButton() const noexcept { return done_button; }

        private:
            struct Pane
            {
                    View               *view = nullptr;
                    std::optional<Rect> area;   ///< as asked for; std::nullopt: the whole window
                    Rect                placed; ///< as laid out
                    bool                on_top = false;
            };
            /// Where views are arranged by hand
            [[nodiscard]] Rect arrangeArea() const noexcept { return work_area.value_or(window_area); }
            /// The panes in drawing order: the ones always on top last
            [[nodiscard]] std::vector<const Pane *> inOrder() const;

            /// The topmost view whose area holds position; nullptr if none
            [[nodiscard]] View                             *viewAt(Point position) const noexcept;
            [[nodiscard]] std::vector<Pane>::const_iterator find(const View &view) const noexcept;
            /// The widgets of all views, again from scratch: after a view came or went
            void rebuildRouter();

            /// A view being moved or resized by hand
            struct Drag
            {
                    PointerId   pointer;
                    const View *view = nullptr;
                    Rect        start; ///< where it was
                    Point       from;  ///< where the pointer went down
                    bool        resize = false;
                    Rect        target; ///< where it would go
                    bool        fits = true;
            };
            void arrangePointer(const PointerEvent &event);
            /// A line around the view the keys go to
            void drawFocus(Canvas &canvas) const;
            void drawArranging(Canvas &canvas) const;

            std::vector<Pane> panes;
            PointerRouter     router;
            View             *focus = nullptr;

            Rect                     window_area;
            std::optional<Rect>      work_area;
            std::optional<Arranging> arrange;
            std::optional<Drag>      drag;
            TextButton               done_button;
            std::optional<PointerId> done_pointer;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_VIEW_HOST_HPP
