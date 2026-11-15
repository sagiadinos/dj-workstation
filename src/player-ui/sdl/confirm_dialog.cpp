// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "confirm_dialog.hpp"

#include "look.hpp"

#include <algorithm>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr float padding    = 20.0F;
        constexpr float row_height = 44.0F;
        constexpr float row_gap    = 6.0F;
        constexpr float button_w   = 160.0F;
        /// Title, message, buttons
        constexpr float rows         = 3.0F;
        constexpr float panel_height = (2 * padding) + (rows * row_height) + ((rows - 1) * row_gap);
    } // namespace

    ConfirmDialog::ConfirmDialog(const Translator &translator, std::string title_key, std::string message_key,
                                 std::string confirm_key)
        : translator{&translator}, title_key{std::move(title_key)}, message_key{std::move(message_key)},
          confirm_key{std::move(confirm_key)}
    {
        cancel_button.setHighlighted(true); // the default, what Enter does
    }

    void ConfirmDialog::layout(const Rect &area)
    {
        window_area       = area;
        const float width = std::min(panel_width, area.width);
        const float high  = std::min(panel_height, area.height);
        panel_area        = {area.x + ((area.width - width) / 2), area.y + ((area.height - high) / 2), width, high};
        blocker.setBounds(area);

        const Rect inner      = panel_area.inset(padding);
        title_area            = {inner.x, inner.y, inner.width, row_height};
        message_area          = {inner.x, inner.y + row_height + row_gap, inner.width, row_height};
        const float buttons_y = inner.y + inner.height - row_height;
        cancel_button.setBounds({inner.x + inner.width - button_w, buttons_y, button_w, row_height});
        confirm_button.setBounds({inner.x + inner.width - (2 * button_w) - 10.0F, buttons_y, button_w, row_height});
    }

    void ConfirmDialog::addTo(PointerRouter &router)
    {
        router.add(blocker); // under everything of the dialog, over everything else
        router.add(confirm_button);
        router.add(cancel_button);
    }

    void ConfirmDialog::update()
    {
        confirm_button.setLabel(translator->tr(confirm_key));
        cancel_button.setLabel(translator->tr("dialog.cancel"));
        if (confirm_button.takePress())
        {
            outcome = Outcome::Confirmed;
        }
        if (cancel_button.takePress())
        {
            outcome = Outcome::Cancelled;
        }
    }

    void ConfirmDialog::keyboard(const KeyboardInput &input)
    {
        const auto *key = std::get_if<EditKey>(&input);
        if (key != nullptr && (*key == EditKey::Escape || *key == EditKey::Enter))
        {
            outcome = Outcome::Cancelled;
        }
    }

    void ConfirmDialog::draw(Canvas &canvas) const
    {
        canvas.fillRect(window_area, {0, 0, 0, 140}); // what lies below does not react
        look::raised(canvas, panel_area, look::colors().button_gray, false, 10.0F);
        const float middle = (row_height - canvas.textHeight()) / 2;
        canvas.textElided({title_area.x, title_area.y + middle}, translator->tr(title_key), look::colors().text,
                          title_area.width - 8.0F);
        canvas.textElided({message_area.x, message_area.y + middle}, translator->tr(message_key), look::colors().text,
                          message_area.width - 8.0F);
        confirm_button.draw(canvas);
        cancel_button.draw(canvas);
    }
} // namespace djw::player_ui::sdl
