// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_ENGINE_AUDIO_SOURCE_HPP
#define DJW_CORE_ENGINE_AUDIO_SOURCE_HPP

#include <cstddef>
#include <span>

namespace djw::core::engine
{
    /// The engine works with interleaved stereo float32 internally.
    inline constexpr std::size_t stereo_channels = 2;

    /**
     * Where a deck gets its audio from.
     *
     * Implementations are called on the audio thread and must be real-time safe. A decoder does not implement this
     * directly; it fills a ring buffer on its own thread and the ring buffer is the source.
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

            /// Writes up to output.size() / 2 frames of interleaved stereo into output. Returns the number of frames
            /// written; fewer than requested means no more audio is available right now.
            virtual std::size_t read(std::span<float> output) noexcept = 0; // [audio-thread]
    };
} // namespace djw::core::engine

#endif // DJW_CORE_ENGINE_AUDIO_SOURCE_HPP
