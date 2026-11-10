// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_API_COMMANDS_HPP
#define DJW_CORE_API_COMMANDS_HPP

#include "api/deck_id.hpp"
#include "api/spsc_queue.hpp"

#include <type_traits>
#include <variant>

namespace djw::core::api
{
    struct DeckPlay
    {
            DeckId deck;
    };

    struct DeckPause
    {
            DeckId deck;
    };

    /// Jumps to a position of the loaded track; clamped to the track
    struct DeckSeek
    {
            DeckId deck;
            double position_seconds;
    };

    /// Channel fader of a deck, 0.0 (silent) to 1.0 (full). Values outside are clamped by the engine.
    struct MixerSetChannelVolume
    {
            DeckId deck;
            float  value;
    };

    /**
     * Everything a frontend or controller can ask the core to do.
     *
     * Commands are plain values without heap memory, so they can pass through a lock-free queue into the audio
     * thread. Keep it that way: a command that needs a string or a buffer (e.g. loading a file) is handled outside
     * the audio thread.
     */
    using Command = std::variant<DeckPlay, DeckPause, DeckSeek, MixerSetChannelVolume>;

    static_assert(std::is_trivially_copyable_v<Command>);

    /// Queue from one producer (a frontend, the controller thread, ...) into the core.
    using CommandQueue = SpscQueue<Command, 256>;
} // namespace djw::core::api

#endif // DJW_CORE_API_COMMANDS_HPP
