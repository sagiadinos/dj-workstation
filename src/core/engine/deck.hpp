// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_ENGINE_DECK_HPP
#define DJW_CORE_ENGINE_DECK_HPP

#include "api/deck_id.hpp"
#include "engine/audio_source.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace djw::core::engine
{
    /**
     * Transport of one deck: plays, pauses and seeks the loaded track and counts the position.
     *
     * Pause fades out over a few milliseconds instead of cutting, so it does not click. During the fade the deck
     * already reports itself as not playing; play() during the fade continues at full level. At the end of the
     * track the deck stops.
     *
     * Volume and mixing are not part of the deck, they belong to its mixer channel.
     */
    class Deck
    {
        public:
            /// pause_fade_frames: length of the fade-out on pause, 0 stops at once
            Deck(api::DeckId id, std::size_t pause_fade_frames) noexcept;

            /// Sets the track; a different track than before starts stopped at its beginning. nullptr unloads.
            void attach(AudioSource *track) noexcept; // [audio-thread]

            [[nodiscard]] api::DeckId   id() const noexcept { return deck_id; }
            [[nodiscard]] bool          hasTrack() const noexcept { return source != nullptr; }
            [[nodiscard]] bool          isPlaying() const noexcept { return state == State::Playing; }
            [[nodiscard]] std::uint64_t positionFrames() const noexcept { return position_frames; }
            [[nodiscard]] std::uint64_t lengthFrames() const noexcept;

            void play() noexcept;                    // [audio-thread]
            void pause() noexcept;                   // [audio-thread]
            void seek(std::uint64_t frame) noexcept; // [audio-thread]

            /// Fills output (interleaved stereo) with the next frames, silence while paused, without a track or when
            /// the audio at the position is not decoded yet.
            void render(std::span<float> output) noexcept; // [audio-thread]

        private:
            enum class State : std::uint8_t
            {
                Stopped,
                Playing,
                FadingOut
            };

            std::size_t renderFadeOut(std::span<float> output) noexcept; // [audio-thread]

            api::DeckId   deck_id;
            AudioSource  *source = nullptr;
            std::size_t   pause_fade_frames;
            std::size_t   fade_remaining  = 0;
            State         state           = State::Stopped;
            std::uint64_t position_frames = 0;
    };
} // namespace djw::core::engine

#endif // DJW_CORE_ENGINE_DECK_HPP
