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
#include "engine/reclaimer.hpp"

#include <atomic>
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
     * Decks and tracks change while rendering: the control thread (the one calling addDeck, removeDeck, setTrack)
     * publishes a new deck list or track through an atomic pointer, the audio thread picks it up at the start of
     * its next render. What it no longer sees is freed later by the control thread, see Reclaimer; call
     * collectGarbage() regularly for that. Command queues are fixed once rendering has started.
     */
    class Engine
    {
        public:
            explicit Engine(EngineConfig config);
            Engine(const Engine &)            = delete;
            Engine &operator=(const Engine &) = delete;
            Engine(Engine &&)                 = delete;
            Engine &operator=(Engine &&)      = delete;
            ~Engine();

            /// One queue per producer (frontend, controller, ...), see SpscQueue. Only before rendering starts.
            api::CommandQueue &addCommandQueue();

            /// Control thread: a new deck without a track
            api::DeckId addDeck();
            /// Control thread: false for an unknown deck. Its snapshot buffer must not be used afterwards.
            bool removeDeck(api::DeckId deck);
            /// Control thread: replaces the deck's track (nullptr unloads); the deck stops at the new track's start.
            /// False for an unknown deck.
            bool setTrack(api::DeckId deck, std::unique_ptr<AudioSource> track);
            /// Control thread: frees decks, lists and tracks the audio thread no longer uses.
            void collectGarbage();

            /// Control thread: snapshot buffer of a deck for the reader side, nullptr for an unknown deck
            api::DeckSnapshotBuffer *deckSnapshot(api::DeckId deck) noexcept;

            [[nodiscard]] std::uint32_t sampleRate() const noexcept { return config.sample_rate; }

            /// Applies pending commands, renders output.size() / 2 frames of interleaved stereo and publishes the
            /// deck snapshots.
            void render(std::span<float> output) noexcept; // [audio-thread]

        private:
            struct DeckSlot
            {
                    DeckSlot(api::DeckId id, std::size_t pause_fade_frames);

                    Deck                    deck;
                    MixerChannel            channel;
                    api::DeckSnapshotBuffer snapshot;
                    /// Published to the audio thread
                    std::atomic<AudioSource *> track{nullptr};
                    /// Control thread: owner of the published track
                    std::unique_ptr<AudioSource> owned_track;
            };

            /// Immutable once published
            struct DeckList
            {
                    std::vector<DeckSlot *> slots;
            };

            DeckSlot *findOwned(api::DeckId deck) noexcept;
            void      publishDeckList();

            static DeckSlot *findDeck(const DeckList &list, api::DeckId deck) noexcept;           // [audio-thread]
            void             applyCommands(const DeckList &list) noexcept;                        // [audio-thread]
            void             apply(const DeckList &list, const api::Command &command) noexcept;   // [audio-thread]
            void             renderBlock(const DeckList &list, std::span<float> output) noexcept; // [audio-thread]
            void             publishSnapshots(const DeckList &list) noexcept;                     // [audio-thread]

            EngineConfig                                    config;
            std::vector<std::unique_ptr<api::CommandQueue>> command_queues;
            std::vector<float>                              deck_buffer;
            Reclaimer                                       reclaimer;
            // Control thread: all decks and the owner of the published list
            std::vector<std::unique_ptr<DeckSlot>> slots;
            std::unique_ptr<DeckList>              current_list;
            std::atomic<const DeckList *>          published_list{nullptr};
            std::uint32_t                          next_deck_id = 1;
    };
} // namespace djw::core::engine

#endif // DJW_CORE_ENGINE_ENGINE_HPP
