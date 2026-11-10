// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_AUDIO_IO_AUDIO_CALLBACK_HPP
#define DJW_CORE_AUDIO_IO_AUDIO_CALLBACK_HPP

#include <span>

namespace djw::core::audio_io
{
    /// What an audio device calls to get the next samples. Runs on the audio thread and must be real-time safe.
    class AudioCallback
    {
        public:
            AudioCallback()                                 = default;
            AudioCallback(const AudioCallback &)            = delete;
            AudioCallback &operator=(const AudioCallback &) = delete;
            AudioCallback(AudioCallback &&)                 = delete;
            AudioCallback &operator=(AudioCallback &&)      = delete;
            virtual ~AudioCallback()                        = default;

            /// Fills output completely with interleaved float32 in the device's channel count (AudioDevice::channels).
            virtual void process(std::span<float> output) noexcept = 0; // [audio-thread]
    };
} // namespace djw::core::audio_io

#endif // DJW_CORE_AUDIO_IO_AUDIO_CALLBACK_HPP
