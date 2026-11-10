// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_API_CORE_HPP
#define DJW_CORE_API_CORE_HPP

#include "api/commands.hpp"
#include "api/deck_id.hpp"
#include "api/snapshot.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>

namespace djw::core::api
{
    struct CoreConfig
    {
            std::uint32_t sample_rate = 48'000;
            /// Audio buffer size; the native target is 128 to 256 frames
            std::uint32_t buffer_frames = 256;
            /// No sound output, the core still runs in real time (tests, headless runs)
            bool silent_output = false;
            /// Tracks up to this length are held in memory, longer ones are streamed (Architecture.md, Track loading)
            std::uint32_t memory_track_limit_seconds = 300;
    };

    /**
     * The core as the frontends see it.
     *
     * Decks are added, removed and loaded through the methods here, also while audio is running. Everything else
     * goes through the frontend's command queue, and deck state comes back through deck snapshots. All methods are
     * called from one thread, the frontend's. The implementation lives in core/runtime; frontends only include this
     * header.
     */
    class Core
    {
        public:
            Core()                        = default;
            Core(const Core &)            = delete;
            Core &operator=(const Core &) = delete;
            Core(Core &&)                 = delete;
            Core &operator=(Core &&)      = delete;
            virtual ~Core()               = default;

            /// Adds an empty deck.
            virtual DeckId addDeck() = 0;

            /// Removes a deck; its snapshot buffer must not be used afterwards.
            virtual std::expected<void, std::string> removeDeck(DeckId deck) = 0;

            /// Loads a file into a deck, replacing its track; the deck stops at the start of the new track. Opening
            /// the file happens here, decoding continues in the background. On error the deck keeps its track.
            virtual std::expected<void, std::string> loadTrack(DeckId deck, const std::filesystem::path &file) = 0;

            /// Starts the audio output.
            virtual std::expected<void, std::string> start() = 0;

            /// Command queue of the frontend (single producer: the frontend's thread)
            virtual CommandQueue &commandQueue() noexcept = 0;

            /// Snapshot of a deck for the frontend (single reader), nullptr for an unknown deck
            virtual DeckSnapshotBuffer *deckSnapshot(DeckId deck) noexcept = 0;
    };

    /// Creates the core with the system's audio output. Implemented in core/runtime.
    std::expected<std::unique_ptr<Core>, std::string> createCore(const CoreConfig &config);
} // namespace djw::core::api

#endif // DJW_CORE_API_CORE_HPP
