// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "audio_io/miniaudio_device.hpp"

#include <miniaudio.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <span>

namespace djw::core::audio_io
{
    namespace
    {
        constexpr ma_uint32 stereo_channels = 2;
        // Master on 1/2, headphones on 3/4
        constexpr ma_uint32 headphone_channels = 4;

        std::string describe(const char *what, ma_result result)
        {
            return std::string{what} + ": " + ma_result_description(result);
        }
    } // namespace

    /// Owns the miniaudio context and device; both are uninitialized exactly once, device first.
    struct MiniaudioDevice::State
    {
            State()                         = default;
            State(const State &)            = delete;
            State &operator=(const State &) = delete;
            State(State &&)                 = delete;
            State &operator=(State &&)      = delete;
            ~State()
            {
                uninitDevice();
                if (context_initialized)
                {
                    ma_context_uninit(&context);
                }
            }

            /// channels 0: the device's own channel count
            ma_result initDevice(const AudioDeviceConfig &config, ma_uint32 channels)
            {
                ma_device_config device_config          = ma_device_config_init(ma_device_type_playback);
                device_config.playback.format           = ma_format_f32;
                device_config.playback.channels         = channels;
                device_config.sampleRate                = config.sample_rate;
                device_config.periodSizeInFrames        = config.buffer_frames;
                device_config.performanceProfile        = ma_performance_profile_low_latency;
                device_config.noPreSilencedOutputBuffer = MA_TRUE; // the callback always writes every sample
                device_config.dataCallback              = &State::dataCallback;
                device_config.pUserData                 = this;

                const ma_result result = ma_device_init(&context, &device_config, &device);
                device_initialized     = result == MA_SUCCESS;
                return result;
            }

            void uninitDevice() noexcept
            {
                if (device_initialized)
                {
                    ma_device_uninit(&device);
                    device_initialized = false;
                }
            }

            static void dataCallback(ma_device *device, void *output, const void *, ma_uint32 frames) // [audio-thread]
            {
                const auto *state   = static_cast<const State *>(device->pUserData);
                const auto  samples = std::span{static_cast<float *>(output),
                                                static_cast<std::size_t>(frames) * device->playback.channels};
                if (AudioCallback *callback = state->callback.load(std::memory_order_acquire))
                {
                    callback->process(samples);
                }
                else
                {
                    std::ranges::fill(samples, 0.0F);
                }
            }

            ma_context                   context{};
            ma_device                    device{};
            bool                         context_initialized = false;
            bool                         device_initialized  = false;
            std::atomic<AudioCallback *> callback{nullptr};
    };

    std::expected<std::unique_ptr<MiniaudioDevice>, std::string> MiniaudioDevice::open(const AudioDeviceConfig &config)
    {
        auto state = std::make_unique<State>();

        const std::array<ma_backend, 1> null_backend{ma_backend_null};
        const bool                      use_null = config.backend == AudioBackend::Null;
        ma_result                       result =
            ma_context_init(use_null ? null_backend.data() : nullptr, use_null ? 1 : 0, nullptr, &state->context);
        if (result != MA_SUCCESS)
        {
            return std::unexpected{describe("cannot initialize audio", result)};
        }
        state->context_initialized = true;

        if (config.headphone_output)
        {
            // The device's own channel count, so miniaudio passes every channel through unmixed. The Null backend
            // has no channel count of its own, it stands in for a 4-channel device.
            const ma_uint32 channels = config.backend == AudioBackend::Null ? headphone_channels : 0;
            if (state->initDevice(config, channels) == MA_SUCCESS &&
                state->device.playback.channels < headphone_channels)
            {
                state->uninitDevice(); // too few channels for headphones: stereo below
            }
        }
        if (!state->device_initialized)
        {
            result = state->initDevice(config, stereo_channels);
            if (result != MA_SUCCESS)
            {
                return std::unexpected{describe("cannot open audio output", result)};
            }
        }

        return std::unique_ptr<MiniaudioDevice>{new MiniaudioDevice{std::move(state)}};
    }

    MiniaudioDevice::MiniaudioDevice(std::unique_ptr<State> state) noexcept : state{std::move(state)} {}

    MiniaudioDevice::~MiniaudioDevice()
    {
        stop();
    }

    std::expected<void, std::string> MiniaudioDevice::start(AudioCallback &callback)
    {
        state->callback.store(&callback, std::memory_order_release);
        const ma_result result = ma_device_start(&state->device);
        if (result != MA_SUCCESS)
        {
            state->callback.store(nullptr, std::memory_order_release);
            return std::unexpected{describe("cannot start audio output", result)};
        }
        return {};
    }

    void MiniaudioDevice::stop() noexcept
    {
        // ma_device_stop returns after the last callback has finished
        if (ma_device_is_started(&state->device) != MA_FALSE)
        {
            ma_device_stop(&state->device);
        }
        state->callback.store(nullptr, std::memory_order_release);
    }

    std::uint32_t MiniaudioDevice::sampleRate() const noexcept
    {
        return state->device.sampleRate;
    }

    std::uint32_t MiniaudioDevice::channels() const noexcept
    {
        return state->device.playback.channels;
    }
} // namespace djw::core::audio_io
