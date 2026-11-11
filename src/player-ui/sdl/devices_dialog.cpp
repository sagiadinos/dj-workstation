// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "devices_dialog.hpp"

#include "look.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <string_view>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr float padding    = 20.0F;
        constexpr float row_height = 44.0F;
        constexpr float button_w   = 140.0F;

        constexpr std::array<std::string_view, latency_choices.size()> latency_keys{
            "dialog.devices.latency_shortest", "dialog.devices.latency_short", "dialog.devices.latency_normal",
            "dialog.devices.latency_safe"};
    } // namespace

    DevicesDialog::DevicesDialog(const Translator &translator, const Settings &settings, std::uint32_t sample_rate)
        : translator{&translator}, settings{settings}, sample_rate{std::max<std::uint32_t>(sample_rate, 1)}
    {
    }

    std::string DevicesDialog::latencyText(std::size_t choice) const
    {
        const double milliseconds =
            1000.0 * static_cast<double>(latency_choices.at(choice)) / static_cast<double>(sample_rate);
        std::string number = std::format("{:.1f}", milliseconds);
        if (translator->language() != Language::English)
        {
            std::ranges::replace(number, '.', ','); // the other languages offered write a decimal comma
        }
        return translator->tr("dialog.devices.latency_entry",
                              {{"name", translator->tr(latency_keys.at(choice))}, {"value", number}});
    }

    void DevicesDialog::layout(const Rect &area)
    {
        window_area       = area;
        const float width = std::min(panel_width, area.width);
        const float high  = std::min(panel_height, area.height);
        panel_area        = {area.x + ((area.width - width) / 2), area.y + ((area.height - high) / 2), width, high};
        blocker.setBounds(area);

        const Rect inner = panel_area.inset(padding);
        // The title takes the first row, then the latency with its label, the hint, the buttons at the bottom
        label_area              = {inner.x, inner.y + row_height, inner.width, row_height};
        const float label_width = std::min(140.0F, inner.width / 3);
        latency.place({inner.x + label_width, label_area.y, inner.width - label_width, row_height}, area);
        hint_area             = {inner.x, label_area.y + row_height + 8.0F, inner.width, 2 * row_height};
        const float buttons_y = inner.y + inner.height - row_height;
        cancel_button.setBounds({inner.x + inner.width - button_w, buttons_y, button_w, row_height});
        ok_button.setBounds({inner.x + inner.width - (2 * button_w) - 10.0F, buttons_y, button_w, row_height});
    }

    void DevicesDialog::addTo(PointerRouter &router)
    {
        router.add(blocker); // under everything of the dialog, over everything else
        router.add(ok_button);
        router.add(cancel_button);
        router.add(latency); // its open list covers the window
    }

    void DevicesDialog::update()
    {
        ok_button.setLabel(translator->tr("dialog.ok"));
        cancel_button.setLabel(translator->tr("dialog.cancel"));
        std::vector<std::string> items;
        items.reserve(latency_choices.size());
        for (std::size_t choice = 0; choice < latency_choices.size(); ++choice)
        {
            items.push_back(latencyText(choice));
        }
        latency.setItems(std::move(items));
        if (const auto picked = latency.takeChosen())
        {
            settings.buffer_frames = latency_choices.at(*picked);
        }
        const auto shown = std::ranges::find(latency_choices, settings.buffer_frames);
        latency.setSelected(shown != latency_choices.end()
                                ? std::optional{static_cast<std::size_t>(shown - latency_choices.begin())}
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

    void DevicesDialog::keyboard(const KeyboardInput &input)
    {
        const auto *key = std::get_if<EditKey>(&input);
        if (key == nullptr)
        {
            return;
        }
        if (*key == EditKey::Escape)
        {
            if (latency.isOpen())
            {
                latency.close();
                return;
            }
            outcome = Outcome::Cancelled;
        }
        else if (*key == EditKey::Enter)
        {
            outcome = Outcome::Accepted;
        }
    }

    void DevicesDialog::draw(Canvas &canvas) const
    {
        canvas.fillRect(window_area, {0, 0, 0, 140}); // what lies below does not react
        look::raised(canvas, panel_area, look::button_gray, false, 10.0F);
        const Rect  inner  = panel_area.inset(padding);
        const float middle = (row_height - canvas.textHeight()) / 2;
        canvas.text({inner.x, inner.y + middle}, translator->tr("dialog.devices.title"), look::text);
        canvas.text({label_area.x, label_area.y + middle}, translator->tr("dialog.devices.latency"), look::text);
        canvas.textElided({hint_area.x, hint_area.y + middle}, translator->tr("dialog.devices.latency_hint"),
                          look::text_dim, hint_area.width);
        canvas.textElided({hint_area.x, hint_area.y + row_height + middle}, translator->tr("dialog.devices.next_start"),
                          look::text_dim, hint_area.width);
        ok_button.draw(canvas);
        cancel_button.draw(canvas);
        latency.draw(canvas); // last: its open list lies over the buttons
    }
} // namespace djw::player_ui::sdl
