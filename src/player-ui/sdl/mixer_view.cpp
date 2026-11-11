// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "mixer_view.hpp"

#include "player_number.hpp"

#include <algorithm>
#include <string>

namespace djw::player_ui::sdl
{
    namespace
    {
        constexpr float margin = 12.0F;
        constexpr float gap    = 10.0F;

        constexpr float row_height    = 44.0F;
        constexpr float side_button   = 48.0F;
        constexpr float zero_width    = 110.0F;
        constexpr float stepper_width = 130.0F;

        constexpr Color label_color{210, 210, 210};
    } // namespace

    MixerView::MixerView(core::api::CommandQueue &queue, core::api::MixerSnapshotBuffer &snapshot,
                         const Translator &translator, Decks decks, Clock clock)
        : queue{&queue}, snapshot{&snapshot}, translator{&translator}, decks{std::move(decks)}, clock{std::move(clock)},
          sent_position{snapshot.read().crossfader_position}
    {
        slider.setValue(sliderValueOf(sent_position));
    }

    void MixerView::layout(const Rect &area)
    {
        const Rect  inner = area.inset(margin);
        const float half  = std::max(0.0F, (inner.width - zero_width - (2 * gap)) / 2);
        float       y     = inner.y;

        side_a.place({inner.x, y, half, row_height}, area);
        zero_button.setBounds({inner.x + half + gap, y, zero_width, row_height});
        side_b.place({inner.x + half + gap + zero_width + gap, y, half, row_height}, area);
        y += row_height + gap;

        button_a.setBounds({inner.x, y, side_button, row_height});
        button_b.setBounds({inner.x + std::max(0.0F, inner.width - side_button), y, side_button, row_height});
        slider.setBounds(
            {inner.x + side_button + gap, y, std::max(0.0F, inner.width - (2 * (side_button + gap))), row_height});
        y += row_height + gap;

        const float fade_width = std::min(inner.width, 220.0F);
        fade_button.setBounds({inner.x + ((inner.width - fade_width) / 2), y, fade_width, row_height});
        y += row_height + gap;

        const float stepper_x = inner.x + std::max(0.0F, inner.width - stepper_width);
        fade_label_area       = {inner.x, y, std::max(0.0F, stepper_x - gap - inner.x), row_height};
        fade_time.setBounds({stepper_x, y, stepper_width, row_height});
    }

    void MixerView::addTo(PointerRouter &router)
    {
        router.add(zero_button);
        router.add(button_a);
        router.add(button_b);
        router.add(slider);
        router.add(fade_button);
        router.add(fade_time);
        // The drop-downs last: an open list covers the window and lies over everything
        router.add(side_a);
        router.add(side_b);
    }

    void MixerView::applyTranslations(const std::vector<MixerDeck> &players)
    {
        zero_button.setLabel(translator->tr("mixer.zero"));
        fade_button.setLabel(translator->tr("mixer.fade_now"));

        std::vector<std::string> names{translator->tr("mixer.none")};
        for (const MixerDeck &player : players)
        {
            names.push_back(translator->tr("mixer.player", {{"number", playerNumber(player.number)}}));
        }
        const auto selection = [&players](core::api::CrossfaderSide side) -> std::size_t
        {
            const auto found = std::ranges::find(players, side, &MixerDeck::side);
            return found == players.end() ? 0 : static_cast<std::size_t>(found - players.begin()) + 1;
        };
        side_a.setItems(names);
        side_b.setItems(std::move(names));
        side_a.setSelected(selection(core::api::CrossfaderSide::A));
        side_b.setSelected(selection(core::api::CrossfaderSide::B));
    }

    void MixerView::assign(const std::vector<MixerDeck> &players, core::api::CrossfaderSide side, std::size_t index)
    {
        // The player that stood on this side steps down
        for (const MixerDeck &player : players)
        {
            if (player.side == side)
            {
                pending.emplace_back(core::api::MixerAssignCrossfader{player.id, core::api::CrossfaderSide::None});
            }
        }
        if (index > 0 && index <= players.size())
        {
            pending.emplace_back(core::api::MixerAssignCrossfader{players[index - 1].id, side});
        }
    }

    void MixerView::handleButtons(const core::api::MixerSnapshot &mixer)
    {
        if (zero_button.takePress())
        {
            fade.reset();
            slider.setValue(sliderValueOf(0.0F));
        }
        for (const auto &[button, side] :
             {std::pair{&button_a, core::api::CrossfaderSide::A}, std::pair{&button_b, core::api::CrossfaderSide::B}})
        {
            if (button->takeDown())
            {
                pending.emplace_back(core::api::MixerCrossfaderMomentary{side});
            }
            if (button->takeUp())
            {
                pending.emplace_back(core::api::MixerCrossfaderMomentary{core::api::CrossfaderSide::None});
            }
        }
        button_a.setHighlighted(mixer.crossfader_momentary == core::api::CrossfaderSide::A);
        button_b.setHighlighted(mixer.crossfader_momentary == core::api::CrossfaderSide::B);

        if (fade_button.takePress())
        {
            const float from = positionOf(slider.value());
            fade             = Fade{.start = clock(), .from = from, .to = from <= 0.0F ? 1.0F : -1.0F};
        }
        // The user moving the slider ends a fade
        if (slider.takeChanged())
        {
            fade.reset();
        }
        static_cast<void>(fade_time.takeChanged());
    }

    void MixerView::stepFade()
    {
        if (!fade)
        {
            return;
        }
        const auto  elapsed  = std::chrono::duration<float>(clock() - fade->start).count();
        const auto  duration = static_cast<float>(fade_time.value());
        const float progress = std::clamp(elapsed / duration, 0.0F, 1.0F);
        slider.setValue(sliderValueOf(fade->from + ((fade->to - fade->from) * progress)));
        if (progress >= 1.0F)
        {
            fade.reset();
        }
    }

    void MixerView::send()
    {
        while (!pending.empty() && queue->tryPush(pending.front()))
        {
            pending.pop_front();
        }
        const float position = positionOf(slider.value());
        if (position != sent_position && queue->tryPush(core::api::MixerSetCrossfader{position}))
        {
            sent_position = position;
        }
    }

    void MixerView::update()
    {
        const core::api::MixerSnapshot mixer   = snapshot->read();
        const std::vector<MixerDeck>   players = decks ? decks() : std::vector<MixerDeck>{};
        applyTranslations(players);

        if (const auto chosen = side_a.takeChosen())
        {
            assign(players, core::api::CrossfaderSide::A, *chosen);
        }
        if (const auto chosen = side_b.takeChosen())
        {
            assign(players, core::api::CrossfaderSide::B, *chosen);
        }
        handleButtons(mixer);

        // The user's slider is the truth for the position, as the channel fader is for the volume; the position of
        // the mixer snapshot is only read at the start
        stepFade();
        send();
    }

    void MixerView::draw(Canvas &canvas) const
    {
        zero_button.draw(canvas);
        button_a.draw(canvas);
        button_b.draw(canvas);
        slider.draw(canvas);
        fade_button.draw(canvas);
        const std::string label = translator->tr("mixer.fade_time");
        canvas.textElided({fade_label_area.x, fade_label_area.y + ((fade_label_area.height - canvas.textHeight()) / 2)},
                          label, label_color, fade_label_area.width);
        fade_time.draw(canvas);
        side_a.draw(canvas);
        side_b.draw(canvas);
    }
} // namespace djw::player_ui::sdl
