// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_MENU_STRIP_HPP
#define DJW_PLAYER_UI_SDL_MENU_STRIP_HPP

#include "menu_bar.hpp"
#include "view.hpp"

#include <functional>

namespace djw::player_ui::sdl
{
    /**
     * A menu bar as a view of its own: the menu bar across the top of the one window of the MDI mode. Its menus
     * drop down over the whole window, not only over the strip; it should lie on top of the other views (see
     * ViewHost::add).
     *
     * The menus and what their entries do stay with the view that owns the menu bar (the library); the strip only
     * places, draws and routes it. Escape closes an open menu.
     */
    class MenuStrip final : public View
    {
        public:
            using WindowArea = std::function<Rect()>;

            /// The menu bar must outlive the strip; window tells the area the menus may drop down over
            MenuStrip(MenuBar &menu, WindowArea window) : menu{&menu}, window{std::move(window)} {}

            void layout(const Rect &area) override { menu->place(area, window ? window() : area); }
            void addTo(PointerRouter &router) override { router.add(*menu); }
            void update() override {}
            void draw(Canvas &canvas) const override { menu->draw(canvas); }
            void keyboard(const KeyboardInput &input) override
            {
                if (const auto *key = std::get_if<EditKey>(&input); key != nullptr && *key == EditKey::Escape)
                {
                    menu->close();
                }
            }

        private:
            MenuBar   *menu;
            WindowArea window;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_MENU_STRIP_HPP
