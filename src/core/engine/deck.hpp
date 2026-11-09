// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_ENGINE_DECK_HPP
#define DJW_CORE_ENGINE_DECK_HPP

#include "api/deck_id.hpp"
#include "engine/audio_source.hpp"

#include <cstdint>
#include <span>

namespace djw::core::engine
{
    /**
     * Transport of one deck: plays or pauses its audio source and counts the position.
     *
     * Volume and mixing are not part of the deck, they belong to its mixer channel.
     */
    class Deck
    {
        public:
            Deck(api::DeckId id, AudioSource &source) noexcept;

            api::DeckId id() const noexcept { return deck_id; }
            bool        isPlaying() const noexcept { return playing; }
            /// Frames played since the start of the source
            std::uint64_t positionFrames() const noexcept { return position_frames; }

            void play() noexcept { playing = true; }   // [audio-thread]
            void pause() noexcept { playing = false; } // [audio-thread]

            /// Fills output (interleaved stereo) with the next frames, silence while paused or when the source runs
            /// dry.
            void render(std::span<float> output) noexcept; // [audio-thread]

        private:
            api::DeckId   deck_id;
            AudioSource  *source;
            bool          playing         = false;
            std::uint64_t position_frames = 0;
    };
} // namespace djw::core::engine

#endif // DJW_CORE_ENGINE_DECK_HPP
