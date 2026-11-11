// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_DROP_DOWN_HPP
#define DJW_PLAYER_UI_SDL_DROP_DOWN_HPP

#include "widget.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace djw::player_ui::sdl
{
    /**
     * A list to choose one entry from. Closed it shows the entry selected; a tap opens the list below it, a tap on an
     * entry chooses it and closes the list, a tap anywhere else only closes it.
     *
     * While the list is open the widget covers the whole window (like the MenuBar), so that tap does not reach what
     * lies below. The owner sets the entries and the selection on every update from the state it shows and reads
     * takeChosen() to learn what the user picked.
     */
    class DropDown final : public Widget
    {
        public:
            static constexpr float item_height = 40.0F;

            void setItems(std::vector<std::string> new_items);
            /// The entry shown as selected; std::nullopt for none
            void setSelected(std::optional<std::size_t> index) noexcept { selected = index; }
            [[nodiscard]] std::optional<std::size_t> selectedIndex() const noexcept { return selected; }

            /// Where the closed drop-down lies and the window the list may open over
            void place(const Rect &closed_area, const Rect &window_area);

            [[nodiscard]] bool isOpen() const noexcept { return open; }
            void               close();

            /// The index of the entry picked, once
            [[nodiscard]] std::optional<std::size_t> takeChosen() noexcept
            {
                return std::exchange(chosen, std::nullopt);
            }

            [[nodiscard]] Rect listArea() const;
            [[nodiscard]] Rect itemArea(std::size_t index) const;

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
                        Box,
                        Item,
                        Outside
                    };
                    Kind        kind  = Kind::None;
                    std::size_t index = 0;

                    bool operator==(const Target &) const = default;
            };

            [[nodiscard]] Target hit(Point position) const;
            void                 refreshBounds();

            std::vector<std::string>   items;
            std::optional<std::size_t> selected;
            Rect                       box;
            Rect                       window;
            bool                       open = false;
            Target                     pressed;
            std::optional<std::size_t> chosen;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_DROP_DOWN_HPP
