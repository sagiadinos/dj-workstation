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
    const std::filesystem::path wav_file  = audio_dir / "sine_440hz_48k_stereo.wav";
    const std::filesystem::path flac_file = audio_dir / "sine_440hz_48k_stereo.flac";

    std::vector<float> decodeFile(const std::filesystem::path &file, std::size_t frames)
    {
        auto decoder = decode::MiniaudioDecoder::open(file, 48'000);
        REQUIRE(decoder.has_value());
        std::vector<float> samples(frames * 2);
        REQUIRE((*decoder)->read(samples) == frames);
        return samples;
    }

    /// A started core with a fake device the test can pull audio from
    struct Fixture
    {
            Fixture() : Fixture{api::CoreConfig{}} {}

            explicit Fixture(const api::CoreConfig &config, std::uint32_t device_rate = 48'000,
                             std::uint32_t device_channels = 2)
            {
                auto owner = std::make_unique<FakeAudioDevice>(device_rate, std::nullopt, device_channels);
                device     = owner.get();
                core       = std::make_unique<runtime::RuntimeCore>(config, std::move(owner));
                REQUIRE(core->start().has_value());
            }

            std::vector<float> pull(std::size_t frames, std::size_t block = 250)
            {
                std::vector<float> output;
                for (std::size_t done = 0; done < frames; done += block)
                {
                    const auto samples = device->pull(std::min(block, frames - done));
                    output.insert(output.end(), samples.begin(), samples.end());
                }
                return output;
            }

            FakeAudioDevice                      *device = nullptr;
            std::unique_ptr<runtime::RuntimeCore> core;
    };
} // namespace

TEST_CASE("RuntimeCore: plays a loaded file through the device after a play command")
{
    Fixture           fixture;
    const api::DeckId deck = fixture.core->addDeck();

    REQUIRE(fixture.core->loadTrack(deck, wav_file).has_value());
    REQUIRE(fixture.core->commandQueue().tryPush(api::DeckPlay{deck}));
    const auto output = fixture.pull(12'000);

    CHECK(output == decodeFile(wav_file, 12'000));
    const api::DeckSnapshot state = fixture.core->deckSnapshot(deck)->read();
    CHECK(state.track_loaded);
    CHECK(state.position_seconds == doctest::Approx(0.25));
    CHECK(state.duration_seconds == doctest::Approx(0.25));
}

TEST_CASE("RuntimeCore: decks are added, loaded and removed while audio is running")
{
    Fixture           fixture;
    const api::DeckId first = fixture.core->addDeck();
    REQUIRE(fixture.core->loadTrack(first, wav_file).has_value());
    REQUIRE(fixture.core->commandQueue().tryPush(api::DeckPlay{first}));
    fixture.pull(1'000);

    const api::DeckId second = fixture.core->addDeck();
    CHECK(second != first);
    REQUIRE(fixture.core->loadTrack(second, flac_file).has_value());
    REQUIRE(fixture.core->commandQueue().tryPush(api::DeckPlay{second}));
    fixture.pull(1'000);
    CHECK(fixture.core->deckSnapshot(second)->read().play_state == api::PlayState::Playing);

    REQUIRE(fixture.core->removeDeck(first).has_value());
    CHECK(fixture.core->deckSnapshot(first) == nullptr);
    fixture.pull(1'000);
    CHECK(fixture.core->deckSnapshot(second)->read().position_seconds == doctest::Approx(2'000.0 / 48'000));
}

TEST_CASE("RuntimeCore: loading into a playing deck stops it at the start of the new track")
{
    Fixture           fixture;
    const api::DeckId deck = fixture.core->addDeck();
    REQUIRE(fixture.core->loadTrack(deck, wav_file).has_value());
    REQUIRE(fixture.core->commandQueue().tryPush(api::DeckPlay{deck}));
    fixture.pull(1'000);

    REQUIRE(fixture.core->loadTrack(deck, flac_file).has_value());
    fixture.pull(250);

    const api::DeckSnapshot state = fixture.core->deckSnapshot(deck)->read();
    CHECK(state.play_state == api::PlayState::Stopped);
    CHECK(state.position_seconds == 0.0);
}

TEST_CASE("RuntimeCore: mixer commands reach the engine and come back in the mixer snapshot")
{
    Fixture fixture;
    REQUIRE(fixture.core->commandQueue().tryPush(api::MixerSetCrossfader{0.5F}));
    REQUIRE(fixture.core->commandQueue().tryPush(api::MixerSetMasterVolume{0.25F}));
    fixture.pull(250);

    const api::MixerSnapshot mixer = fixture.core->mixerSnapshot().read();
    CHECK(mixer.crossfader_position == 0.5F);
    CHECK(mixer.master_volume == 0.25F);
}

TEST_CASE("RuntimeCore: a 4-channel device gets the master on 1/2 and the headphones on 3/4")
{
    Fixture           fixture{{}, 48'000, 4};
    const api::DeckId deck = fixture.core->addDeck();
    REQUIRE(fixture.core->loadTrack(deck, wav_file).has_value());
    REQUIRE(fixture.core->commandQueue().tryPush(api::MixerSetChannelVolume{deck, 0.0F}));
    REQUIRE(fixture.core->commandQueue().tryPush(api::MixerSetHeadphoneCue{deck, true}));
    REQUIRE(fixture.core->commandQueue().tryPush(api::DeckPlay{deck}));
    fixture.pull(250); // faders ramp

    const auto output   = fixture.pull(1'000);
    const auto expected = decodeFile(wav_file, 1'250);
    REQUIRE(output.size() == 1'000 * 4);
    for (std::size_t frame = 0; frame < 1'000; ++frame)
    {
        CHECK(output[frame * 4] == 0.0F);
        CHECK(output[(frame * 4) + 1] == 0.0F);
        CHECK(output[(frame * 4) + 2] == expected[(250 + frame) * 2]);
        CHECK(output[(frame * 4) + 3] == expected[((250 + frame) * 2) + 1]);
    }
    CHECK(fixture.core->mixerSnapshot().read().headphones_available);
}

TEST_CASE("RuntimeCore: the engine runs at the sample rate of the device")
{
    Fixture           fixture{{.sample_rate = 48'000}, 44'100};
    const api::DeckId deck = fixture.core->addDeck();
    REQUIRE(fixture.core->loadTrack(deck, wav_file).has_value());
    REQUIRE(fixture.core->commandQueue().tryPush(api::DeckPlay{deck}));

    fixture.pull(4'410);

    CHECK(fixture.core->deckSnapshot(deck)->read().position_seconds == doctest::Approx(0.1));
}

TEST_CASE("RuntimeCore: errors name the problem and leave the deck as it was")
{
    Fixture           fixture;
    const api::DeckId deck = fixture.core->addDeck();
    REQUIRE(fixture.core->loadTrack(deck, wav_file).has_value());

    const auto missing = fixture.core->loadTrack(deck, audio_dir / "missing.flac");
    REQUIRE_FALSE(missing.has_value());
    CHECK(missing.error().find("missing.flac") != std::string::npos);
    fixture.pull(250);
    CHECK(fixture.core->deckSnapshot(deck)->read().track_loaded);

    CHECK_FALSE(fixture.core->loadTrack(api::DeckId{99}, wav_file).has_value());
    CHECK_FALSE(fixture.core->removeDeck(api::DeckId{99}).has_value());
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

TEST_CASE("createCore: with silent output a deck plays in real time")
{
    auto core = api::createCore({.silent_output = true});
    REQUIRE_MESSAGE(core.has_value(), core.error());

    const api::DeckId deck = (*core)->addDeck();
    REQUIRE((*core)->loadTrack(deck, wav_file).has_value());
    REQUIRE((*core)->commandQueue().tryPush(api::DeckPlay{deck}));
    REQUIRE((*core)->start().has_value());

    api::DeckSnapshotBuffer *snapshot = (*core)->deckSnapshot(deck);
    const auto               deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
    while (snapshot->read().position_seconds < 0.05 && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
    }

    CHECK(snapshot->read().position_seconds >= 0.05);
}
