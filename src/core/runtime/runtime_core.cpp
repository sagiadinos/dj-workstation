// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "runtime/runtime_core.hpp"

#include <string>
#include <utility>

namespace djw::core::runtime
{
    namespace
    {
        // Decoded audio buffered for a streamed track; covers slow storage such as SD cards on the Raspberry Pi
        constexpr std::uint32_t stream_buffer_seconds = 1;

        std::string unknownDeck(api::DeckId deck)
        {
            return "unknown deck " + std::to_string(std::to_underlying(deck));
        }
    } // namespace

    RuntimeCore::RuntimeCore(const api::CoreConfig &config, std::unique_ptr<audio_io::AudioDevice> audio_device)
        : track_config{.sample_rate = audio_device->sampleRate(),
                       .memory_limit_frames =
                           std::uint64_t{config.memory_track_limit_seconds} * audio_device->sampleRate(),
                       .stream_buffer_frames = std::size_t{stream_buffer_seconds} * audio_device->sampleRate()},
          engine{{.sample_rate = audio_device->sampleRate(), .max_block_frames = config.buffer_frames}},
          frontend_queue{engine.addCommandQueue()}, callback{engine}, device{std::move(audio_device)}
    {
    }

    RuntimeCore::~RuntimeCore()
    {
        device->stop();
    }

    api::DeckId RuntimeCore::addDeck()
    {
        engine.collectGarbage();
        return engine.addDeck();
    }

    std::expected<void, std::string> RuntimeCore::removeDeck(api::DeckId deck)
    {
        engine.collectGarbage();
        if (!engine.removeDeck(deck))
        {
            return std::unexpected{unknownDeck(deck)};
        }
        return {};
    }

    std::expected<void, std::string> RuntimeCore::loadTrack(api::DeckId deck, const std::filesystem::path &file)
    {
        engine.collectGarbage();
        if (engine.deckSnapshot(deck) == nullptr)
        {
            return std::unexpected{unknownDeck(deck)};
        }
        auto track = decode::openTrack(file, track_config);
        if (!track)
        {
            return std::unexpected{track.error()};
        }
        engine.setTrack(deck, std::move(*track));
        return {};
    }

    std::expected<void, std::string> RuntimeCore::start()
    {
        if (started)
        {
            return {};
        }
        auto result = device->start(callback);
        started     = result.has_value();
        return result;
    }

    api::DeckSnapshotBuffer *RuntimeCore::deckSnapshot(api::DeckId deck) noexcept
    {
        return engine.deckSnapshot(deck);
    }

    void RuntimeCore::EngineCallback::process(std::span<float> output) noexcept // [audio-thread]
    {
        engine->render(output);
    }
} // namespace djw::core::runtime
