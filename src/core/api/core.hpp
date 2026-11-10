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
#include <optional>
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
            /**
             * Master on channels 1/2 and headphones on 3/4 of the output device (e.g. Hercules DJControl Starlight).
             * The device is opened with its own channel count; with fewer than 4 channels the output falls back to
             * master only (MixerSnapshot::headphones_available). Off: stereo master only.
             */
            bool headphone_output = false;
            /// Tracks up to this length are held in memory, longer ones are streamed (Architecture.md, Track loading)
            std::uint32_t memory_track_limit_seconds = 300;
    };

    /// How loadTrack starts the deck
    struct LoadOptions
    {
            /// Start here and make it the cue point, e.g. the one stored for the track; ignored beyond the track's end
            std::optional<double> cue_seconds{};
            /// Without cue_seconds: start at the first audible sound if it is known (the beginning of the track if
            /// not) and make that the cue point. Off: start at the beginning.
            bool cue_to_first_sound = false;
    };

    /// What loadTrack made of a track
    struct LoadedTrack
    {
            /// Where the deck stands and its cue point, in seconds (0 at the beginning)
            double cue_seconds = 0.0;
            /// The first audible sound, when it was looked for and found; the core looks in the first 30 seconds
            std::optional<double> first_sound_seconds{};
    };

    /**
     * The core as the frontends see it.
     *
     * Decks are added, removed and loaded through the methods here, also while audio is running. Everything else
     * goes through the frontend's command queue, and state comes back through the deck and mixer snapshots. All methods
     * are called from one thread, the frontend's. The implementation lives in core/runtime; frontends only include this
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

            /// Loads a file into a deck, replacing its track; the deck stops at the start of the new track, or where
            /// options say. Opening the file happens here, decoding continues in the background. On error the deck
            /// keeps its track. Do not load a deck that plays: the frontend decides that.
            virtual std::expected<LoadedTrack, std::string> loadTrack(DeckId deck, const std::filesystem::path &file,
                                                                      const LoadOptions &options = {}) = 0;

            /// Starts the audio output.
            virtual std::expected<void, std::string> start() = 0;

            /// Command queue of the frontend (single producer: the frontend's thread)
            virtual CommandQueue &commandQueue() noexcept = 0;

            /// Snapshot of a deck for the frontend (single reader), nullptr for an unknown deck
            virtual DeckSnapshotBuffer *deckSnapshot(DeckId deck) noexcept = 0;

            /// Snapshot of the mixer master section for the frontend (single reader)
            virtual MixerSnapshotBuffer &mixerSnapshot() noexcept = 0;
    };

    /// Creates the core with the system's audio output. Implemented in core/runtime.
    std::expected<std::unique_ptr<Core>, std::string> createCore(const CoreConfig &config);
} // namespace djw::core::api

#endif // DJW_CORE_API_CORE_HPP
