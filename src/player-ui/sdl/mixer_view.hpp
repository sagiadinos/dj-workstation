// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_MIXER_VIEW_HPP
#define DJW_PLAYER_UI_SDL_MIXER_VIEW_HPP

#include "api/commands.hpp"
#include "api/crossfader_side.hpp"
#include "api/deck_id.hpp"
#include "api/snapshot.hpp"
#include "clock.hpp"
#include "drop_down.hpp"
#include "i18n/translator.hpp"
#include "slider.hpp"
#include "stepper.hpp"
#include "text_button.hpp"
#include "view.hpp"

#include <chrono>
#include <deque>
#include <functional>
#include <optional>
#include <vector>

namespace djw::player_ui::sdl
{
    /// A deck the crossfader window can offer for a side, as the app knows it
    struct MixerDeck
    {
            core::api::DeckId         id;
            /// Shown as "Player 01"
            int                       number = 0;
            core::api::CrossfaderSide side   = core::api::CrossfaderSide::None;
    };

    /**
     * The crossfader window: the player on side A (left drop-down) and on side B (right drop-down), Zero (crossfader
     * to the center), the momentary A and B buttons (the crossfader acts as if it were at that end while held), the
     * crossfader itself, "Fade now" (moves the crossfader to the other side within the fade time) and the fade time
     * in seconds.
     *
     * A drop-down lists the players with a "(none)" entry first. Choosing a player for a side takes it away from the
     * player that was there; a player stands on one side only. The state shown comes from the mixer snapshot and the
     * players the owner reports, the changes go out as commands.
     */
    class MixerView final : public View
    {
        public:
            using Decks = std::function<std::vector<MixerDeck>()>;

            static constexpr int min_fade_seconds     = 1;
            static constexpr int max_fade_seconds     = 60;
            static constexpr int default_fade_seconds = 5;

            MixerView(core::api::CommandQueue &queue, core::api::MixerSnapshotBuffer &snapshot,
                      const Translator &translator, Decks decks, Clock clock = steadyClock());

            void layout(const Rect &area) override;
            void addTo(PointerRouter &router) override;
            void update() override;
            void draw(Canvas &canvas) const override;

            [[nodiscard]] const DropDown   &sideA() const noexcept { return side_a; }
            [[nodiscard]] const DropDown   &sideB() const noexcept { return side_b; }
            [[nodiscard]] const TextButton &zeroButton() const noexcept { return zero_button; }
            [[nodiscard]] const TextButton &momentaryA() const noexcept { return button_a; }
            [[nodiscard]] const TextButton &momentaryB() const noexcept { return button_b; }
            [[nodiscard]] const TextButton &fadeButton() const noexcept { return fade_button; }
            [[nodiscard]] const Slider     &crossfader() const noexcept { return slider; }
            [[nodiscard]] const Stepper    &fadeTime() const noexcept { return fade_time; }
            [[nodiscard]] bool              fading() const noexcept { return fade.has_value(); }

            /// The crossfader position of a slider value and back: -1 (A) ... +1 (B)
            [[nodiscard]] static float positionOf(float slider_value) noexcept { return (slider_value * 2.0F) - 1.0F; }
            [[nodiscard]] static float sliderValueOf(float position) noexcept { return (position + 1.0F) / 2.0F; }

        private:
            struct Fade
            {
                    std::chrono::steady_clock::time_point start;
                    float                                 from = 0.0F;
                    float                                 to   = 0.0F;
            };

            void applyTranslations(const std::vector<MixerDeck> &decks);
            /// Puts the player chosen in drop-down index (0 is "(none)") on side
            void assign(const std::vector<MixerDeck> &decks, core::api::CrossfaderSide side, std::size_t index);
            void handleButtons(const core::api::MixerSnapshot &mixer);
            void stepFade();
            void send();

            core::api::CommandQueue        *queue;
            core::api::MixerSnapshotBuffer *snapshot;
            const Translator               *translator;
            Decks                           decks;
            Clock                           clock;

            DropDown   side_a;
            DropDown   side_b;
            TextButton zero_button;
            TextButton button_a{"A"};
            TextButton button_b{"B"};
            Slider     slider;
            TextButton fade_button;
            Stepper    fade_time{min_fade_seconds, max_fade_seconds, default_fade_seconds};
            Rect       fade_label_area;

            std::optional<Fade>            fade;
            float                          sent_position = 0.0F;
            std::deque<core::api::Command> pending;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_MIXER_VIEW_HPP
