// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_ENGINE_AUDIO_SOURCE_HPP
#define DJW_CORE_ENGINE_AUDIO_SOURCE_HPP

#include <cstddef>
#include <cstdint>
#include <span>

namespace djw::core::engine
{
    /// The engine works with interleaved stereo float32 internally.
    inline constexpr std::size_t stereo_channels = 2;

    /**
     * A loaded track as the deck sees it: audio with a read position that can be moved.
     *
     * Everything here is called on the audio thread and must be real-time safe. Decoding happens elsewhere; an
     * implementation only hands out what is already decoded.
     */
    class AudioSource
    {
        public:
            AudioSource()                               = default;
            AudioSource(const AudioSource &)            = delete;
            AudioSource &operator=(const AudioSource &) = delete;
            AudioSource(AudioSource &&)                 = delete;
            AudioSource &operator=(AudioSource &&)      = delete;
            virtual ~AudioSource()                      = default;

            /// Writes up to output.size() / 2 frames of interleaved stereo into output and advances the read position.
            /// Returns the number of frames written; fewer than requested means the end is reached or the audio at
            /// the position is not decoded yet.
            virtual std::size_t read(std::span<float> output) noexcept = 0; // [audio-thread]

            /// Moves the read position, clamped to the length.
            virtual void seek(std::uint64_t frame) noexcept = 0; // [audio-thread]

            /// Length in frames, 0 if unknown
            [[nodiscard]] virtual std::uint64_t lengthFrames() const noexcept = 0; // [audio-thread]

            /// The read position is at the end; nothing more will come without a seek.
            [[nodiscard]] virtual bool atEnd() const noexcept = 0; // [audio-thread]

            /// How much of the track is in memory and can be played without waiting, 0 to 1; 1 for a source that has
            /// everything at hand or does not load in parts. Counts the decoded chunks, so it need not be exact when
            /// the user jumped ahead.
            [[nodiscard]] virtual float decodedFraction() const noexcept { return 1.0F; } // [audio-thread]

            /// Where a deck starts when it takes this source and where it puts its cue point (0: the beginning), e.g.
            /// the first sound. Set before the source is handed to the engine; the engine only reads it.
            void                        setStartFrame(std::uint64_t frame) noexcept { start_frame = frame; }
            [[nodiscard]] std::uint64_t startFrame() const noexcept { return start_frame; }

        private:
            std::uint64_t start_frame = 0;
    };
} // namespace djw::core::engine

#endif // DJW_CORE_ENGINE_AUDIO_SOURCE_HPP
