// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "language_dialog.hpp"

#include "look.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr float padding    = 20.0F;
        constexpr float row_height = 44.0F;
        constexpr float row_gap    = 6.0F;
        constexpr float button_w   = 140.0F;
        /// Title, list, a free row (an open list hangs over it), buttons
        constexpr float rows         = 4.0F;
        constexpr float panel_height = (2 * padding) + (rows * row_height) + ((rows - 1) * row_gap);
    } // namespace

    LanguageDialog::LanguageDialog(const Translator &translator, Language current)
        : translator{&translator}, language{current}
    {
    }

    void LanguageDialog::layout(const Rect &area)
    {
        window_area       = area;
        const float width = std::min(panel_width, area.width);
        const float high  = std::min(panel_height, area.height);
        panel_area        = {area.x + ((area.width - width) / 2), area.y + ((area.height - high) / 2), width, high};
        blocker.setBounds(area);

        const Rect inner = panel_area.inset(padding);
        title_area       = {inner.x, inner.y, inner.width, row_height};
        list.place({inner.x, inner.y + row_height + row_gap, inner.width, row_height}, area);
        const float buttons_y = inner.y + inner.height - row_height;
        cancel_button.setBounds({inner.x + inner.width - button_w, buttons_y, button_w, row_height});
        ok_button.setBounds({inner.x + inner.width - (2 * button_w) - 10.0F, buttons_y, button_w, row_height});
    }

    void LanguageDialog::addTo(PointerRouter &router)
    {
        router.add(blocker); // under everything of the dialog, over everything else
        router.add(ok_button);
        router.add(cancel_button);
        router.add(list); // last: an open list lies over the buttons
    }

    void LanguageDialog::update()
    {
        ok_button.setLabel(translator->tr("dialog.ok"));
        cancel_button.setLabel(translator->tr("dialog.cancel"));
        if (const auto picked = list.takeChosen(); picked && *picked < languages.size())
        {
            language = languages.at(*picked);
        }

        std::vector<std::string> names;
        names.reserve(languages.size());
        for (const Language each : languages)
        {
            names.emplace_back(languageName(each));
        }
        list.setItems(std::move(names));
        list.setSelected(static_cast<std::size_t>(std::ranges::find(languages, language) - languages.begin()));

        if (ok_button.takePress())
        {
            outcome = Outcome::Accepted;
        }
        if (cancel_button.takePress())
        {
            outcome = Outcome::Cancelled;
        }
    }

    void LanguageDialog::keyboard(const KeyboardInput &input)
    {
        const auto *key = std::get_if<EditKey>(&input);
        if (key == nullptr)
        {
            return;
        }
        if (*key == EditKey::Escape)
        {
            if (list.isOpen())
            {
                list.close();
            }
            else
            {
                outcome = Outcome::Cancelled;
            }
        }
        else if (*key == EditKey::Enter)
        {
            outcome = Outcome::Accepted;
        }
    }

    void LanguageDialog::draw(Canvas &canvas) const
    {
        canvas.fillRect(window_area, {0, 0, 0, 140}); // what lies below does not react
        look::raised(canvas, panel_area, look::button_gray, false, 10.0F);
        canvas.textElided({title_area.x, title_area.y + ((row_height - canvas.textHeight()) / 2)},
                          translator->tr("dialog.language.title"), look::text, title_area.width - 8.0F);
        ok_button.draw(canvas);
        cancel_button.draw(canvas);
        list.draw(canvas); // last: an open list lies over the buttons
    }
} // namespace djw::player_ui::sdl
