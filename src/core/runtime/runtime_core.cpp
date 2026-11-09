// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "runtime/runtime_core.hpp"

#include "decode/miniaudio_decoder.hpp"

namespace djw::core::runtime
{
    namespace
    {
        // Decoded audio buffered per deck; covers slow storage such as SD cards on the Raspberry Pi
        constexpr std::uint32_t stream_buffer_seconds = 1;
    } // namespace

    RuntimeCore::RuntimeCore(const api::CoreConfig &config, std::unique_ptr<audio_io::AudioDevice> audio_device)
        : engine{{.sample_rate = audio_device->sampleRate(), .max_block_frames = config.buffer_frames}},
          frontend_queue{engine.addCommandQueue()}, callback{engine}, device{std::move(audio_device)}
    {
    }

    RuntimeCore::~RuntimeCore()
    {
        device->stop();
    }

    std::expected<api::DeckId, std::string> RuntimeCore::addDeck(const std::filesystem::path &file)
    {
        if (started)
        {
            return std::unexpected{std::string{"decks can only be added before start"}};
        }
        auto decoder = decode::MiniaudioDecoder::open(file, engine.sampleRate());
        if (!decoder)
        {
            return std::unexpected{decoder.error()};
        }
        sources.push_back(std::make_unique<decode::StreamingSource>(std::move(*decoder),
                                                                    stream_buffer_seconds * engine.sampleRate()));
        return engine.addDeck(*sources.back());
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
