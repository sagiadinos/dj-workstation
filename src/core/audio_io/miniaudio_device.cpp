// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "audio_io/miniaudio_device.hpp"

#include <miniaudio.h>

#include <algorithm>
#include <atomic>
#include <span>

namespace djw::core::audio_io
{
    namespace
    {
        constexpr ma_uint32 output_channels = 2;

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
                if (device_initialized)
                {
                    ma_device_uninit(&device);
                }
                if (context_initialized)
                {
                    ma_context_uninit(&context);
                }
            }

            static void dataCallback(ma_device *device, void *output, const void *, ma_uint32 frames) // [audio-thread]
            {
                auto      *state = static_cast<State *>(device->pUserData);
                const auto samples =
                    std::span{static_cast<float *>(output), static_cast<std::size_t>(frames) * output_channels};
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

        const ma_backend null_backend[] = {ma_backend_null};
        const bool       use_null       = config.backend == AudioBackend::Null;
        ma_result        result =
            ma_context_init(use_null ? null_backend : nullptr, use_null ? 1 : 0, nullptr, &state->context);
        if (result != MA_SUCCESS)
        {
            return std::unexpected{describe("cannot initialize audio", result)};
        }
        state->context_initialized = true;

        ma_device_config device_config          = ma_device_config_init(ma_device_type_playback);
        device_config.playback.format           = ma_format_f32;
        device_config.playback.channels         = output_channels;
        device_config.sampleRate                = config.sample_rate;
        device_config.periodSizeInFrames        = config.buffer_frames;
        device_config.performanceProfile        = ma_performance_profile_low_latency;
        device_config.noPreSilencedOutputBuffer = MA_TRUE; // the callback always writes every sample
        device_config.dataCallback              = &State::dataCallback;
        device_config.pUserData                 = state.get();

        result = ma_device_init(&state->context, &device_config, &state->device);
        if (result != MA_SUCCESS)
        {
            return std::unexpected{describe("cannot open audio output", result)};
        }
        state->device_initialized = true;

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
} // namespace djw::core::audio_io
