// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "pane_frame.hpp"

#include "look.hpp"

#include <algorithm>
#include <utility>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr Color bar_color{34, 35, 38};
        constexpr float text_inset = 10.0F;
        constexpr float button_gap = 3.0F;
    } // namespace

    PaneFrame::PaneFrame(View &content, Title title, Close close)
        : content{&content}, title{std::move(title)}, close{std::move(close)}
    {
    }

    void PaneFrame::layout(const Rect &outer)
    {
        whole = outer;
        // Title bar and content lie inside the border
        const Rect  area       = outer.inset(border);
        const float bar_height = std::min(title_height, area.height);
        title_bar              = {area.x, area.y, area.width, bar_height};
        if (close)
        {
            const float side = std::max(0.0F, bar_height - (2 * button_gap));
            close_button.setBounds({area.x + area.width - side - button_gap, area.y + button_gap, side, side});
        }
        else
        {
            close_button.setBounds({}); // hidden
        }
        content->layout({area.x, area.y + bar_height, area.width, std::max(0.0F, area.height - bar_height)});
    }

    void PaneFrame::addTo(PointerRouter &router)
    {
        router.add(close_button);
        content->addTo(router);
    }

    void PaneFrame::update()
    {
        content->update();
        if (close_button.takePress() && close)
        {
            close();
        }
    }

    void PaneFrame::draw(Canvas &canvas) const
    {
        canvas.fillRect(title_bar, bar_color);
        if (title)
        {
            const float room = title_bar.width - (2 * text_inset) - (close ? title_height : 0.0F);
            const float y    = title_bar.y + ((title_bar.height - canvas.textHeight()) / 2);
            canvas.textElided({title_bar.x + text_inset, y}, title(), look::text, std::max(0.0F, room));
        }
        close_button.draw(canvas);
        content->draw(canvas);
        // The border, last, so the content cannot cover it
        canvas.fillRect({whole.x, whole.y, whole.width, border}, border_color);
        canvas.fillRect({whole.x, whole.y + whole.height - border, whole.width, border}, border_color);
        canvas.fillRect({whole.x, whole.y, border, whole.height}, border_color);
        canvas.fillRect({whole.x + whole.width - border, whole.y, border, whole.height}, border_color);
    }
} // namespace djw::player_ui::sdl
