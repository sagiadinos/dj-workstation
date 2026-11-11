// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "devices_dialog.hpp"

#include "look.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <ranges>
#include <string_view>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr float padding     = 20.0F;
        constexpr float row_height  = 44.0F;
        constexpr float row_gap     = 6.0F;
        constexpr float button_w    = 140.0F;
        constexpr float label_share = 0.3F; ///< of the inner width, the labels in front of the lists

        constexpr std::array<std::string_view, latency_choices.size()> latency_keys{
            "dialog.devices.latency_shortest", "dialog.devices.latency_short", "dialog.devices.latency_normal",
            "dialog.devices.latency_safe"};
    } // namespace

    DevicesDialog::DevicesDialog(const Translator &translator, Settings settings, std::uint32_t sample_rate,
                                 DeviceChoices choices)
        : translator{&translator}, settings{std::move(settings)}, sample_rate{std::max<std::uint32_t>(sample_rate, 1)},
          choices{std::move(choices)}
    {
        // An output chosen before that is not there now stays in the list, so it is not lost by accident
        if (!this->settings.output_device.empty() &&
            std::ranges::find(this->choices.outputs, this->settings.output_device) == this->choices.outputs.end())
        {
            this->choices.outputs.push_back(this->settings.output_device);
        }
        this->choices.ports.resize(std::min(this->choices.ports.size(), max_ports));
        this->choices.port_defaults.resize(this->choices.ports.size());
        for (std::size_t port = 0; port < this->choices.ports.size(); ++port)
        {
            mapping_lists.push_back(std::make_unique<DropDown>());
        }
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

    float DevicesDialog::panelHeight() const noexcept
    {
        // Title, output, channels, latency, two lines of hint, the controllers' heading and lines, the buttons
        const auto  ports = static_cast<float>(std::max<std::size_t>(choices.ports.size(), 1));
        const float rows  = 4.0F + 2.0F + 1.0F + ports + 1.0F;
        return (2 * padding) + (rows * row_height) + ((rows - 1) * row_gap);
    }

    std::string DevicesDialog::mappingOf(std::size_t port) const
    {
        if (const auto chosen = settings.controllers.find(choices.ports.at(port)); chosen != settings.controllers.end())
        {
            return chosen->second;
        }
        return choices.port_defaults.at(port);
    }

    void DevicesDialog::layout(const Rect &area)
    {
        window_area       = area;
        const float width = std::min(panel_width, area.width);
        const float high  = std::min(panelHeight(), area.height);
        panel_area        = {area.x + ((area.width - width) / 2), area.y + ((area.height - high) / 2), width, high};
        blocker.setBounds(area);

        const Rect  inner = panel_area.inset(padding);
        const float label = inner.width * label_share;
        float       y     = inner.y;
        const auto  next  = [&y](float rows = 1.0F)
        {
            const float at = y;
            y += (rows * row_height) + row_gap;
            return at;
        };
        title_area = {inner.x, next(), inner.width, row_height};
        // Audio: a label, the list after it
        for (auto [label_area, list] : {std::pair{&output_label, &output}, std::pair{&channels_label, &channels},
                                        std::pair{&latency_label, &latency}})
        {
            const float at = next();
            *label_area    = {inner.x, at, label, row_height};
            list->place({inner.x + label, at, inner.width - label, row_height}, area);
        }
        hint_area        = {inner.x, next(2.0F), inner.width, 2 * row_height};
        controllers_area = {inner.x, next(), inner.width, row_height};
        port_labels.clear();
        for (std::size_t port = 0; port < choices.ports.size(); ++port)
        {
            const float at = next();
            port_labels.push_back({inner.x, at, label, row_height});
            mapping_lists.at(port)->place({inner.x + label, at, inner.width - label, row_height}, area);
        }
        const float buttons_y = inner.y + inner.height - row_height;
        cancel_button.setBounds({inner.x + inner.width - button_w, buttons_y, button_w, row_height});
        ok_button.setBounds({inner.x + inner.width - (2 * button_w) - 10.0F, buttons_y, button_w, row_height});
    }

    void DevicesDialog::addTo(PointerRouter &router)
    {
        router.add(blocker); // under everything of the dialog, over everything else
        router.add(ok_button);
        router.add(cancel_button);
        // The lists last, the lower ones first: an open list covers the window and lies over the lists below it
        for (const auto &list : std::views::reverse(mapping_lists))
        {
            router.add(*list);
        }
        router.add(latency);
        router.add(channels);
        router.add(output);
    }

    void DevicesDialog::takeChoices()
    {
        if (const auto picked = output.takeChosen())
        {
            settings.output_device = *picked == 0 ? std::string{} : choices.outputs.at(*picked - 1);
        }
        if (const auto picked = channels.takeChosen())
        {
            settings.headphone_output = *picked == 1;
        }
        if (const auto picked = latency.takeChosen())
        {
            settings.buffer_frames = latency_choices.at(*picked);
        }
        for (std::size_t port = 0; port < mapping_lists.size(); ++port)
        {
            if (const auto picked = mapping_lists.at(port)->takeChosen())
            {
                settings.controllers[choices.ports.at(port)] =
                    *picked == 0 ? std::string{} : choices.mappings.at(*picked - 1);
            }
        }
    }

    void DevicesDialog::update()
    {
        ok_button.setLabel(translator->tr("dialog.ok"));
        cancel_button.setLabel(translator->tr("dialog.cancel"));
        takeChoices();

        std::vector<std::string> outputs{translator->tr("dialog.devices.output_automatic")};
        outputs.insert(outputs.end(), choices.outputs.begin(), choices.outputs.end());
        output.setItems(std::move(outputs));
        const auto shown_output = std::ranges::find(choices.outputs, settings.output_device);
        output.setSelected(settings.output_device.empty() || shown_output == choices.outputs.end()
                               ? 0
                               : static_cast<std::size_t>(shown_output - choices.outputs.begin()) + 1);

        channels.setItems(
            {translator->tr("dialog.devices.channels_stereo"), translator->tr("dialog.devices.channels_four")});
        channels.setSelected(settings.headphone_output ? 1 : 0);

        std::vector<std::string> latencies;
        latencies.reserve(latency_choices.size());
        for (std::size_t choice = 0; choice < latency_choices.size(); ++choice)
        {
            latencies.push_back(latencyText(choice));
        }
        latency.setItems(std::move(latencies));
        const auto shown_latency = std::ranges::find(latency_choices, settings.buffer_frames);
        latency.setSelected(shown_latency != latency_choices.end()
                                ? std::optional{static_cast<std::size_t>(shown_latency - latency_choices.begin())}
                                : std::nullopt);

        for (std::size_t port = 0; port < mapping_lists.size(); ++port)
        {
            std::vector<std::string> names{translator->tr("dialog.devices.no_mapping")};
            names.insert(names.end(), choices.mappings.begin(), choices.mappings.end());
            mapping_lists.at(port)->setItems(std::move(names));
            const std::string uses   = mappingOf(port);
            const auto        mapped = std::ranges::find(choices.mappings, uses);
            mapping_lists.at(port)->setSelected(uses.empty() || mapped == choices.mappings.end()
                                                    ? 0
                                                    : static_cast<std::size_t>(mapped - choices.mappings.begin()) + 1);
        }

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
            bool closed = false;
            for (DropDown *list : {&output, &channels, &latency})
            {
                closed = closed || list->isOpen();
                list->close();
            }
            for (const auto &list : mapping_lists)
            {
                closed = closed || list->isOpen();
                list->close();
            }
            if (!closed)
            {
                outcome = Outcome::Cancelled;
            }
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
        const float middle = (row_height - canvas.textHeight()) / 2;
        const auto  text   = [&canvas, middle](const Rect &area, const std::string &line, Color color)
        { canvas.textElided({area.x, area.y + middle}, line, color, area.width - 8.0F); };
        text(title_area, translator->tr("dialog.devices.title"), look::text);
        text(output_label, translator->tr("dialog.devices.output"), look::text);
        text(channels_label, translator->tr("dialog.devices.channels"), look::text);
        text(latency_label, translator->tr("dialog.devices.latency"), look::text);
        text(hint_area, translator->tr("dialog.devices.latency_hint"), look::text_dim);
        text({hint_area.x, hint_area.y + row_height, hint_area.width, row_height},
             translator->tr("dialog.devices.next_start"), look::text_dim);
        text(controllers_area,
             translator->tr(choices.ports.empty() ? "dialog.devices.no_controllers" : "dialog.devices.controllers"),
             look::text);
        for (std::size_t port = 0; port < port_labels.size(); ++port)
        {
            text(port_labels.at(port), choices.ports.at(port), look::text_dim);
        }
        ok_button.draw(canvas);
        cancel_button.draw(canvas);
        // The lists last, the upper ones last: an open list lies over the lists below it
        for (const auto &list : std::views::reverse(mapping_lists))
        {
            list->draw(canvas);
        }
        latency.draw(canvas);
        channels.draw(canvas);
        output.draw(canvas);
    }
} // namespace djw::player_ui::sdl
