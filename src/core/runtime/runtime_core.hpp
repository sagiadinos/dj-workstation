// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_RUNTIME_RUNTIME_CORE_HPP
#define DJW_CORE_RUNTIME_RUNTIME_CORE_HPP

#include "api/core.hpp"
#include "audio_io/audio_callback.hpp"
#include "audio_io/audio_device.hpp"
#include "decode/track_loader.hpp"
#include "engine/engine.hpp"

#include <memory>

namespace djw::core::runtime
{
    /**
     * Implementation of api::Core: wires engine, track loading and the audio device together.
     *
     * The device is injected, so tests can drive the core with a fake device. It is stopped before anything it
     * calls into is destroyed. Every call frees what the engine retired since the last call (unloaded tracks,
     * removed decks).
     */
    class RuntimeCore final : public api::Core
    {
        public:
            RuntimeCore(const api::CoreConfig &config, std::unique_ptr<audio_io::AudioDevice> audio_device);
            RuntimeCore(const RuntimeCore &)            = delete;
            RuntimeCore &operator=(const RuntimeCore &) = delete;
            RuntimeCore(RuntimeCore &&)                 = delete;
            RuntimeCore &operator=(RuntimeCore &&)      = delete;
            ~RuntimeCore() override;

            api::DeckId                      addDeck() override;
            std::expected<void, std::string> removeDeck(api::DeckId deck) override;
            std::expected<void, std::string> loadTrack(api::DeckId deck, const std::filesystem::path &file) override;
            std::expected<void, std::string> start() override;
            api::CommandQueue               &commandQueue() noexcept override { return frontend_queue; }
            api::DeckSnapshotBuffer         *deckSnapshot(api::DeckId deck) noexcept override;

        private:
            /// Lets the audio device call the engine
            class EngineCallback final : public audio_io::AudioCallback
            {
                public:
                    explicit EngineCallback(engine::Engine &engine) noexcept : engine{&engine} {}
                    void process(std::span<float> output) noexcept override; // [audio-thread]

                private:
                    engine::Engine *engine;
            };

            decode::TrackLoadConfig track_config;
            engine::Engine          engine;
            api::CommandQueue      &frontend_queue;
            EngineCallback          callback;
            bool                    started = false;
            // Declared last: destroyed first
            std::unique_ptr<audio_io::AudioDevice> device;
    };
} // namespace djw::core::runtime

#endif // DJW_CORE_RUNTIME_RUNTIME_CORE_HPP
