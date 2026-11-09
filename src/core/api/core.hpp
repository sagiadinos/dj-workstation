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
    };

    /**
     * The core as the frontends see it.
     *
     * Setup goes through the methods here (adding decks, starting the audio output). While running, a frontend
     * only pushes commands into its command queue and reads deck snapshots. The implementation lives in
     * core/runtime; frontends only include this header.
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

            /// Adds a deck with the given file loaded. Only before start(); adding decks while running comes with M1.
            virtual std::expected<DeckId, std::string> addDeck(const std::filesystem::path &file) = 0;

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
