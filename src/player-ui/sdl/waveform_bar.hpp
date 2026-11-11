// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_PLAYER_UI_SDL_WAVEFORM_BAR_HPP
#define DJW_PLAYER_UI_SDL_WAVEFORM_BAR_HPP

#include "widget.hpp"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace djw::player_ui::sdl
{
    /**
     * The whole track as a waveform, with the play position and the cue point; touching or dragging it chooses a
     * position (takeSeek()). Without a waveform (the track is not analyzed yet, or comes from outside the library)
     * it is a plain bar that works the same way.
     *
     * The waveform is a row of values from 0 (silent) to 255, spread evenly over the track. The part already played
     * is drawn in yellow, the rest in blue.
     */
    class WaveformBar final : public Widget
    {
        public:
            /// Replaces the waveform; empty for none
            void                             setWaveform(std::vector<std::uint8_t> peaks) { waveform = std::move(peaks); }
            [[nodiscard]] bool               hasWaveform() const noexcept { return !waveform.empty(); }
            [[nodiscard]] const std::vector<std::uint8_t> &peaks() const noexcept { return waveform; }

            /// The play position as a share of the track (0 to 1)
            void               setPosition(float fraction) noexcept;
            [[nodiscard]] float position() const noexcept { return played; }
            /// The cue point as a share of the track; std::nullopt for none
            void setCue(std::optional<float> fraction) noexcept { cue = fraction; }

            /// A finger or the mouse is on the bar
            [[nodiscard]] bool held() const noexcept { return is_held; }
            /// The share of the track the user chose since the last call, std::nullopt if nothing was chosen
            [[nodiscard]] std::optional<float> takeSeek() noexcept { return std::exchange(chosen, std::nullopt); }

            void pointerDown(Point position) override;
            void pointerMove(Point position) override;
            void pointerUp(Point position) override;

            void draw(Canvas &canvas) const override;

        private:
            static constexpr float frame = 3.0F;

            [[nodiscard]] Rect inner() const noexcept { return bounds().inset(frame); }
            void               follow(Point position) noexcept;

            std::vector<std::uint8_t> waveform;
            std::optional<float>      cue;
            std::optional<float>      chosen;
            float                     played  = 0.0F;
            bool                      is_held = false;
    };
} // namespace djw::player_ui::sdl

#endif // DJW_PLAYER_UI_SDL_WAVEFORM_BAR_HPP
