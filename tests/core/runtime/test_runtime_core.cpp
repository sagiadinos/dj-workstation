// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "runtime/runtime_core.hpp"

#include "decode/miniaudio_decoder.hpp"
#include "fake_audio_device.hpp"

#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <thread>

using namespace djw::core;
using djw::tests::FakeAudioDevice;

namespace
{
    const std::filesystem::path audio_dir{DJW_TEST_AUDIO_DIR};
    const std::filesystem::path wav_file = audio_dir / "sine_440hz_48k_stereo.wav";

    std::vector<float> decodeFile(const std::filesystem::path &file, std::size_t frames)
    {
        auto decoder = decode::MiniaudioDecoder::open(file, 48'000);
        REQUIRE(decoder.has_value());
        std::vector<float> samples(frames * 2);
        REQUIRE((*decoder)->read(samples) == frames);
        return samples;
    }
} // namespace

TEST_CASE("RuntimeCore: plays a file through the device after a play command")
{
    auto                 device_owner = std::make_unique<FakeAudioDevice>();
    auto                &device       = *device_owner;
    runtime::RuntimeCore core{{}, std::move(device_owner)};

    const auto deck = core.addDeck(wav_file);
    REQUIRE_MESSAGE(deck.has_value(), deck.error());
    REQUIRE(core.commandQueue().tryPush(api::DeckPlay{*deck}));
    REQUIRE(core.start().has_value());

    std::vector<float> output;
    for (int block = 0; block < 12'000 / 250; ++block)
    {
        const auto samples = device.pull(250);
        output.insert(output.end(), samples.begin(), samples.end());
    }

    CHECK(output == decodeFile(wav_file, 12'000));
    CHECK(core.deckSnapshot(*deck)->read().play_state == api::PlayState::Playing);
    CHECK(core.deckSnapshot(*deck)->read().position_seconds == doctest::Approx(0.25));
}

TEST_CASE("RuntimeCore: the engine runs at the sample rate of the device")
{
    auto                 device_owner = std::make_unique<FakeAudioDevice>(44'100);
    auto                &device       = *device_owner;
    runtime::RuntimeCore core{{.sample_rate = 48'000}, std::move(device_owner)};

    const auto deck = core.addDeck(wav_file);
    REQUIRE(deck.has_value());
    REQUIRE(core.commandQueue().tryPush(api::DeckPlay{*deck}));
    REQUIRE(core.start().has_value());
    device.pull(4'410);

    CHECK(core.deckSnapshot(*deck)->read().position_seconds == doctest::Approx(0.1));
}

TEST_CASE("RuntimeCore: a file that cannot be decoded is an error naming the file")
{
    runtime::RuntimeCore core{{}, std::make_unique<FakeAudioDevice>()};

    const auto deck = core.addDeck(audio_dir / "missing.flac");

    REQUIRE_FALSE(deck.has_value());
    CHECK(deck.error().find("missing.flac") != std::string::npos);
}

TEST_CASE("RuntimeCore: decks cannot be added after start")
{
    runtime::RuntimeCore core{{}, std::make_unique<FakeAudioDevice>()};
    REQUIRE(core.start().has_value());

    CHECK_FALSE(core.addDeck(wav_file).has_value());
}

TEST_CASE("RuntimeCore: an error of the device on start is passed on")
{
    runtime::RuntimeCore core{{}, std::make_unique<FakeAudioDevice>(48'000, "device busy")};

    const auto started = core.start();

    REQUIRE_FALSE(started.has_value());
    CHECK(started.error() == "device busy");
}

TEST_CASE("RuntimeCore: the device is stopped when the core is destroyed")
{
    auto device = std::make_unique<FakeAudioDevice>();
    auto log    = device->sharedLog();
    {
        runtime::RuntimeCore core{{}, std::move(device)};
        REQUIRE(core.start().has_value());
        CHECK(log->started);
        CHECK_FALSE(log->stopped);
    }
    CHECK(log->stopped);
}

TEST_CASE("createCore: with silent output the deck plays in real time")
{
    auto core = api::createCore({.silent_output = true});
    REQUIRE_MESSAGE(core.has_value(), core.error());

    const auto deck = (*core)->addDeck(wav_file);
    REQUIRE(deck.has_value());
    REQUIRE((*core)->commandQueue().tryPush(api::DeckPlay{*deck}));
    REQUIRE((*core)->start().has_value());

    api::DeckSnapshotBuffer *snapshot = (*core)->deckSnapshot(*deck);
    const auto               deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
    while (snapshot->read().position_seconds < 0.05 && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
    }

    CHECK(snapshot->read().play_state == api::PlayState::Playing);
    CHECK(snapshot->read().position_seconds >= 0.05);
}
