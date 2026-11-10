// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_CONTEXT_MENU_HPP
#define DJW_PLAYER_UI_SDL_CONTEXT_MENU_HPP

#include "menu_bar.hpp"
#include "widget.hpp"

#include <optional>
#include <utility>
#include <vector>

namespace djw::player_ui::sdl
{
    /**
     * A menu that opens at a point, e.g. where the right mouse button went down on a track.
     *
     * Like the menu bar it covers the whole window while it is open, so a touch anywhere else only closes it; a choice
     * counts when the pointer goes down and up on the same entry. The view sets the (translated) items on every update.
     * It is moved so that it lies inside the window.
     */
    class ContextMenu final : public Widget
    {
        public:
            static constexpr float item_height = 44.0F;
            static constexpr float menu_width  = 260.0F;

            void setItems(std::vector<MenuItem> new_items);
            /// The window the menu may open in
            void place(const Rect &window_area);

            /// Opens at position; ignored without items
            void open(Point position);
            void close();
            [[nodiscard]] bool isOpen() const noexcept { return opened; }

            /// The id of the entry chosen, once
            [[nodiscard]] std::optional<int> takeChosen() noexcept { return std::exchange(chosen, std::nullopt); }

            [[nodiscard]] Rect menuArea() const;
            [[nodiscard]] Rect itemArea(std::size_t index) const;

            void pointerDown(Point position) override;
            void pointerMove(Point /*position*/) override {}
            void pointerUp(Point position) override;

            void draw(Canvas &canvas) const override;

        private:
            static constexpr std::size_t none = static_cast<std::size_t>(-1);
            static constexpr std::size_t outside = none - 1;

            /// Index of the entry under position, outside for the rest of the window
            [[nodiscard]] std::size_t hit(Point position) const;

            std::vector<MenuItem> items;
            Rect                  window;
            Point                 at;
            bool                  opened = false;
            std::size_t           pressed = none;
            std::optional<int>    chosen;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_CONTEXT_MENU_HPP
