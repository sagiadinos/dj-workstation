// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_AUDIO_IO_AUDIO_DEVICE_HPP
#define DJW_CORE_AUDIO_IO_AUDIO_DEVICE_HPP

#include "audio_io/audio_callback.hpp"

#include <cstdint>
#include <expected>
#include <string>

namespace djw::core::audio_io
{
    /**
     * An audio output that pulls interleaved float32 from a callback at a fixed sample rate and channel count.
     *
     * Behind this interface so the core can be tested with a fake device that is driven by the test.
     */
    class AudioDevice
    {
        public:
            AudioDevice()                               = default;
            AudioDevice(const AudioDevice &)            = delete;
            AudioDevice &operator=(const AudioDevice &) = delete;
            AudioDevice(AudioDevice &&)                 = delete;
            AudioDevice &operator=(AudioDevice &&)      = delete;
            virtual ~AudioDevice()                      = default;

            /// Starts calling callback on the audio thread. The callback must outlive the running device.
            virtual std::expected<void, std::string> start(AudioCallback &callback) = 0;
            /// Stops the device; when it returns, the callback is no longer running.
            virtual void stop() noexcept = 0;

            [[nodiscard]] virtual std::uint32_t sampleRate() const noexcept = 0;
            /// Interleaved channels the callback fills, at least 2
            [[nodiscard]] virtual std::uint32_t channels() const noexcept = 0;
    };
} // namespace djw::core::audio_io

#endif // DJW_CORE_AUDIO_IO_AUDIO_DEVICE_HPP
