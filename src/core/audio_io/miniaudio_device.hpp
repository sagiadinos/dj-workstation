// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_AUDIO_IO_MINIAUDIO_DEVICE_HPP
#define DJW_CORE_AUDIO_IO_MINIAUDIO_DEVICE_HPP

#include "audio_io/audio_device.hpp"

#include <cstdint>
#include <expected>
#include <memory>
#include <string>

namespace djw::core::audio_io
{
    enum class AudioBackend : std::uint8_t
    {
        /// The default output of the system (PipeWire/PulseAudio/ALSA, WASAPI, CoreAudio, ...)
        System,
        /// No sound, but called in real time like a real device; for tests and headless runs
        Null
    };

    struct AudioDeviceConfig
    {
            std::uint32_t sample_rate   = 48'000;
            std::uint32_t buffer_frames = 256;
            AudioBackend  backend       = AudioBackend::System;
            /// Master and headphones: open the device with its own channel count, at least 4 (for the Null backend:
            /// exactly 4); a device with fewer channels is opened as stereo instead. Off: stereo.
            bool headphone_output = false;
    };

    /**
     * Audio output through miniaudio.
     *
     * miniaudio converts to the format of the hardware if needed, so the callback always gets the requested sample
     * rate in float32. In stereo, miniaudio maps to the device's channels; with headphone output the device is used
     * with its own channel count, so no channel is mixed into another. miniaudio.h stays out of this header.
     */
    class MiniaudioDevice final : public AudioDevice
    {
        public:
            static std::expected<std::unique_ptr<MiniaudioDevice>, std::string> open(const AudioDeviceConfig &config);

            ~MiniaudioDevice() override;

            std::expected<void, std::string> start(AudioCallback &callback) override;
            void                             stop() noexcept override;
            [[nodiscard]] std::uint32_t      sampleRate() const noexcept override;
            [[nodiscard]] std::uint32_t      channels() const noexcept override;

        private:
            struct State;

            explicit MiniaudioDevice(std::unique_ptr<State> state) noexcept;

            std::unique_ptr<State> state;
    };
} // namespace djw::core::audio_io

#endif // DJW_CORE_AUDIO_IO_MINIAUDIO_DEVICE_HPP
