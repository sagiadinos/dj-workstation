// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_TESTS_SUPPORT_FAKE_AUDIO_DEVICE_HPP
#define DJW_TESTS_SUPPORT_FAKE_AUDIO_DEVICE_HPP

#include "audio_io/audio_device.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace djw::tests
{
    /// What happened to a FakeAudioDevice; shared, so a test can still look at it after the device is destroyed.
    struct FakeAudioDeviceLog
    {
            bool started = false;
            bool stopped = false;
    };

    /**
     * Audio device driven by the test: nothing runs on its own, pull() calls the callback on the test's thread.
     */
    class FakeAudioDevice final : public core::audio_io::AudioDevice
    {
        public:
            explicit FakeAudioDevice(std::uint32_t              sample_rate   = 48'000,
                                     std::optional<std::string> start_error   = std::nullopt,
                                     std::uint32_t              channel_count = 2)
                : sample_rate{sample_rate}, channel_count{channel_count}, start_error{std::move(start_error)}
            {
            }

            std::expected<void, std::string> start(core::audio_io::AudioCallback &audio_callback) override
            {
                if (start_error)
                {
                    return std::unexpected{*start_error};
                }
                callback     = &audio_callback;
                log->started = true;
                return {};
            }

            void stop() noexcept override
            {
                callback     = nullptr;
                log->stopped = true;
            }

            std::uint32_t sampleRate() const noexcept override { return sample_rate; }
            std::uint32_t channels() const noexcept override { return channel_count; }

            /// Calls the callback for frames frames of channels() channels, like the audio thread would; silence while
            /// not started.
            std::vector<float> pull(std::size_t frames)
            {
                std::vector<float> output(frames * channel_count, 0.0F);
                if (callback != nullptr)
                {
                    callback->process(output);
                }
                return output;
            }

            std::shared_ptr<FakeAudioDeviceLog> sharedLog() const { return log; }

        private:
            std::uint32_t                       sample_rate;
            std::uint32_t                       channel_count;
            std::optional<std::string>          start_error;
            core::audio_io::AudioCallback      *callback = nullptr;
            std::shared_ptr<FakeAudioDeviceLog> log      = std::make_shared<FakeAudioDeviceLog>();
    };
} // namespace djw::tests

#endif // DJW_TESTS_SUPPORT_FAKE_AUDIO_DEVICE_HPP
