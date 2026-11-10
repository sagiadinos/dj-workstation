// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "audio_io/miniaudio_device.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>

using namespace djw::core::audio_io;

namespace
{
    // Counts what the device asks for; writes a constant, so the output buffer is touched like a real callback does
    class CountingCallback final : public AudioCallback
    {
        public:
            void process(std::span<float> output) noexcept override
            {
                std::ranges::fill(output, 0.25F);
                samples.fetch_add(output.size(), std::memory_order_relaxed);
                partial_frame = partial_frame.load(std::memory_order_relaxed) || output.size() % channels != 0;
            }

            std::size_t              channels = 2;
            std::atomic<std::size_t> samples{0};
            std::atomic<bool>        partial_frame{false};
    };

    std::unique_ptr<MiniaudioDevice> openNullDevice(bool headphone_output = false)
    {
        auto device = MiniaudioDevice::open({.sample_rate      = 48'000,
                                             .buffer_frames    = 256,
                                             .backend          = AudioBackend::Null,
                                             .headphone_output = headphone_output});
        REQUIRE_MESSAGE(device.has_value(), device.error());
        return std::move(*device);
    }

    /// Waits until the callback has been called, at most two seconds
    void waitForCallbacks(const CountingCallback &callback)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
        while (callback.samples.load() == 0 && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{5});
        }
    }
} // namespace

TEST_CASE("MiniaudioDevice: runs at the requested sample rate")
{
    CHECK(openNullDevice()->sampleRate() == 48'000);
}

TEST_CASE("MiniaudioDevice: calls the callback with whole stereo frames while started")
{
    auto             device = openNullDevice();
    CountingCallback callback;

    REQUIRE(device->start(callback).has_value());
    waitForCallbacks(callback);
    device->stop();

    CHECK(callback.samples.load() > 0);
    CHECK_FALSE(callback.partial_frame.load());
}

TEST_CASE("MiniaudioDevice: stereo by default, 4 channels with headphone output")
{
    CHECK(openNullDevice()->channels() == 2);

    auto             device = openNullDevice(true);
    CountingCallback callback;
    callback.channels = 4;
    CHECK(device->channels() == 4);

    REQUIRE(device->start(callback).has_value());
    waitForCallbacks(callback);
    device->stop();

    CHECK(callback.samples.load() > 0);
    CHECK_FALSE(callback.partial_frame.load());
}

TEST_CASE("MiniaudioDevice: after stop the callback is no longer called")
{
    auto             device = openNullDevice();
    CountingCallback callback;
    REQUIRE(device->start(callback).has_value());
    waitForCallbacks(callback);

    device->stop();
    const std::size_t after_stop = callback.samples.load();
    std::this_thread::sleep_for(std::chrono::milliseconds{50});

    CHECK(callback.samples.load() == after_stop);
}

TEST_CASE("MiniaudioDevice: can be destroyed while running and stopped without being started")
{
    CountingCallback callback;
    {
        auto device = openNullDevice();
        REQUIRE(device->start(callback).has_value());
        waitForCallbacks(callback);
    }
    {
        auto device = openNullDevice();
        device->stop();
    }
    CHECK(callback.samples.load() > 0);
}
