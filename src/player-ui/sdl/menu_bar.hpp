// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_MENU_BAR_HPP
#define DJW_PLAYER_UI_SDL_MENU_BAR_HPP

#include "widget.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace djw::player_ui::sdl
{
    /// One entry of a drop-down menu; id says which entry was chosen
    struct MenuItem
    {
            int         id = 0;
            std::string label;
            /// A disabled entry is shown dimmed and cannot be chosen
            bool enabled = true;
            /// Shown highlighted, e.g. the language in use
            bool checked = false;
    };

    struct Menu
    {
            std::string           title;
            std::vector<MenuItem> items;
    };

    /**
     * A menu bar with drop-down menus, made for touch: large targets, a choice counts when the pointer goes down and
     * up on the same entry.
     *
     * The view sets the menus (translated, with the current enabled and checked states) on every update; an open menu
     * stays open if its position still exists. While a menu is open the widget covers the whole window, so a touch
     * anywhere else only closes the menu and does not reach what lies below.
     *
     * In right-to-left layout the titles start at the right edge and a drop-down is aligned to the right edge of its
     * title.
     */
    class MenuBar final : public Widget
    {
        public:
            static constexpr float bar_height      = 40.0F;
            static constexpr float title_width     = 150.0F;
            static constexpr float item_height     = 44.0F;
            static constexpr float dropdown_width  = 340.0F;

            void setMenus(std::vector<Menu> menus_);
            void setRightToLeft(bool right_to_left) noexcept { rtl = right_to_left; }

            /// Where the bar lies and the window it may drop down over
            void place(const Rect &bar_area, const Rect &window_area);

            [[nodiscard]] bool                        isOpen() const noexcept { return open.has_value(); }
            [[nodiscard]] std::optional<std::size_t>  openMenu() const noexcept { return open; }
            void                                      close();

            /// The id of the entry chosen, once
            [[nodiscard]] std::optional<int> takeChosen() noexcept { return std::exchange(chosen, std::nullopt); }

            /// Where the title and the drop-down of a menu are drawn
            [[nodiscard]] Rect titleArea(std::size_t menu) const;
            [[nodiscard]] Rect dropdownArea(std::size_t menu) const;
            [[nodiscard]] Rect itemArea(std::size_t menu, std::size_t item) const;

            void pointerDown(Point position) override;
            void pointerMove(Point /*position*/) override {}
            void pointerUp(Point position) override;

            void draw(Canvas &canvas) const override;

        private:
            struct Target
            {
                    enum class Kind : unsigned char
                    {
                        None,
                        Title,
                        Item,
                        Outside
                    };
                    Kind        kind  = Kind::None;
                    std::size_t index = 0;

                    bool operator==(const Target &) const = default;
            };

            [[nodiscard]] Target hit(Point position) const;
            void                 refreshBounds();

            std::vector<Menu>          menus;
            Rect                       bar;
            Rect                       window;
            bool                       rtl = false;
            std::optional<std::size_t> open;
            Target                     pressed;
            std::optional<int>         chosen;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_MENU_BAR_HPP
