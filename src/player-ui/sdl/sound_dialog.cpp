// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "sound_dialog.hpp"

#include "look.hpp"

#include <algorithm>
#include <string>
#include <string_view>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr float padding     = 20.0F;
        constexpr float row_height  = 44.0F;
        constexpr float row_gap     = 6.0F;
        constexpr float button_w    = 140.0F;
        constexpr float label_share = 0.4F; ///< of the inner width, the label in front of the list
        /// Title, quality, the hint, a free row (an open list hangs over it), buttons
        constexpr float rows         = 5.0F;
        constexpr float panel_height = (2 * padding) + (rows * row_height) + ((rows - 1) * row_gap);

        std::string_view qualityKey(core::api::TimeStretch library)
        {
            return library == core::api::TimeStretch::Signalsmith ? "dialog.sound.quality_high"
                                                                  : "dialog.sound.quality_standard";
        }
    } // namespace

    SoundDialog::SoundDialog(const Translator &translator, Settings settings,
                             std::vector<core::api::TimeStretch> offered)
        : translator{&translator}, settings{std::move(settings)}, offered{std::move(offered)}
    {
    }

    void SoundDialog::layout(const Rect &area)
    {
        window_area       = area;
        const float width = std::min(panel_width, area.width);
        const float high  = std::min(panel_height, area.height);
        panel_area        = {area.x + ((area.width - width) / 2), area.y + ((area.height - high) / 2), width, high};
        blocker.setBounds(area);

        const Rect  inner = panel_area.inset(padding);
        const float label = inner.width * label_share;
        const float row   = row_height + row_gap;
        title_area        = {inner.x, inner.y, inner.width, row_height};
        quality_label     = {inner.x, inner.y + row, label, row_height};
        quality.place({inner.x + label, inner.y + row, inner.width - label, row_height}, area);
        hint_area             = {inner.x, inner.y + (2 * row), inner.width, row_height};
        const float buttons_y = inner.y + inner.height - row_height;
        cancel_button.setBounds({inner.x + inner.width - button_w, buttons_y, button_w, row_height});
        ok_button.setBounds({inner.x + inner.width - (2 * button_w) - 10.0F, buttons_y, button_w, row_height});
    }

    void SoundDialog::addTo(PointerRouter &router)
    {
        router.add(blocker); // under everything of the dialog, over everything else
        router.add(ok_button);
        router.add(cancel_button);
        router.add(quality); // last: an open list lies over the buttons
    }

    void SoundDialog::update()
    {
        ok_button.setLabel(translator->tr("dialog.ok"));
        cancel_button.setLabel(translator->tr("dialog.cancel"));
        if (const auto picked = quality.takeChosen(); picked && *picked < offered.size())
        {
            settings.time_stretch = offered.at(*picked);
        }

        std::vector<std::string> names;
        names.reserve(offered.size());
        for (const core::api::TimeStretch library : offered)
        {
            names.push_back(translator->tr(qualityKey(library)));
        }
        quality.setItems(std::move(names));
        // A setting from a build with a library this one lacks: nothing chosen, the setting kept
        const auto shown = std::ranges::find(offered, settings.time_stretch);
        quality.setSelected(shown != offered.end() ? std::optional{static_cast<std::size_t>(shown - offered.begin())}
                                                   : std::nullopt);

        if (ok_button.takePress())
        {
            outcome = Outcome::Accepted;
        }
        if (cancel_button.takePress())
        {
            outcome = Outcome::Cancelled;
        }
    }

    void SoundDialog::keyboard(const KeyboardInput &input)
    {
        const auto *key = std::get_if<EditKey>(&input);
        if (key == nullptr)
        {
            return;
        }
        if (*key == EditKey::Escape)
        {
            if (quality.isOpen())
            {
                quality.close();
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

    void SoundDialog::draw(Canvas &canvas) const
    {
        canvas.fillRect(window_area, {0, 0, 0, 140}); // what lies below does not react
        look::raised(canvas, panel_area, look::button_gray, false, 10.0F);
        const float middle = (row_height - canvas.textHeight()) / 2;
        const auto  text   = [&canvas, middle](const Rect &area, const std::string &line, Color color)
        { canvas.textElided({area.x, area.y + middle}, line, color, area.width - 8.0F); };
        text(title_area, translator->tr("dialog.sound.title"), look::text);
        text(quality_label, translator->tr("dialog.sound.keylock_quality"), look::text);
        text(hint_area, translator->tr("dialog.devices.next_start"), look::text_dim);
        ok_button.draw(canvas);
        cancel_button.draw(canvas);
        quality.draw(canvas); // last: an open list lies over the buttons
    }
} // namespace djw::player_ui::sdl
