// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_ENGINE_ENGINE_HPP
#define DJW_CORE_ENGINE_ENGINE_HPP

#include "api/commands.hpp"
#include "api/deck_id.hpp"
#include "api/snapshot.hpp"
#include "engine/audio_source.hpp"
#include "engine/deck.hpp"
#include "engine/mixer_channel.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace djw::core::engine
{
    struct EngineConfig
    {
            std::uint32_t sample_rate = 48'000;
            /// Largest block rendered in one piece; larger requests are split. Sizes the internal buffers.
            std::size_t max_block_frames = 1'024;
    };

    /**
     * The audio engine: decks, their mixer channels and the master sum.
     *
     * render() is the audio callback. It runs without an audio device too, which is what the offline render tests
     * use. Commands come in through command queues, deck state goes out through one snapshot buffer per deck.
     *
     * Setup (addCommandQueue, addDeck) must happen before rendering starts. Adding and removing decks while
     * rendering comes with M1 (deck list swapped through an atomic pointer).
     */
    class Engine
    {
        public:
            explicit Engine(EngineConfig config);

            /// One queue per producer (frontend, controller, ...), see SpscQueue.
            api::CommandQueue &addCommandQueue();
            api::DeckId        addDeck(AudioSource &source);

            /// Snapshot buffer of a deck for the reader side, nullptr for an unknown deck
            api::DeckSnapshotBuffer *deckSnapshot(api::DeckId deck) noexcept;

            std::uint32_t sampleRate() const noexcept { return config.sample_rate; }

            /// Applies pending commands, renders output.size() / 2 frames of interleaved stereo and publishes the
            /// deck snapshots.
            void render(std::span<float> output) noexcept; // [audio-thread]

        private:
            struct DeckSlot
            {
                    DeckSlot(api::DeckId id, AudioSource &source);

                    Deck                    deck;
                    MixerChannel            channel;
                    api::DeckSnapshotBuffer snapshot;
            };

            DeckSlot *findDeck(api::DeckId deck) noexcept;
            void      applyCommands() noexcept;                      // [audio-thread]
            void      apply(const api::Command &command) noexcept;   // [audio-thread]
            void      renderBlock(std::span<float> output) noexcept; // [audio-thread]
            void      publishSnapshots() noexcept;                   // [audio-thread]

            EngineConfig                                    config;
            std::vector<std::unique_ptr<api::CommandQueue>> command_queues;
            std::vector<std::unique_ptr<DeckSlot>>          decks;
            std::vector<float>                              deck_buffer;
            std::uint32_t                                   next_deck_id = 1;
    };
} // namespace djw::core::engine

#endif // DJW_CORE_ENGINE_ENGINE_HPP
