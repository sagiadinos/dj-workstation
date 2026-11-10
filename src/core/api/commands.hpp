// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_API_COMMANDS_HPP
#define DJW_CORE_API_COMMANDS_HPP

#include "api/crossfader_side.hpp"
#include "api/deck_id.hpp"
#include "api/spsc_queue.hpp"

#include <cstdint>
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

    /**
     * The CUE button, sent on press and on release. Classic CDJ behavior: while playing, the deck jumps back to the
     * cue point and stops; when stopped, the cue point is set to the current position; when stopped at the cue
     * point, the deck plays while the button is held and returns to the cue point on release. Play while holding
     * CUE keeps the deck playing after release.
     */
    struct DeckCue
    {
            DeckId deck;
            bool   pressed;
    };

    /// Pitch fader in percent, clamped to the pitch range (±12 %). Changes tempo and pitch together.
    struct DeckSetPitch
    {
            DeckId deck;
            float  percent;
    };

    /// Pitch bend buttons: -1 slower, +1 faster while held, 0 on release
    struct DeckPitchBend
    {
            DeckId      deck;
            std::int8_t direction;
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

    /// Puts a deck on crossfader side A, B or none
    struct MixerAssignCrossfader
    {
            DeckId         deck;
            CrossfaderSide side;
    };

    /**
     * Crossfader position from -1.0 (side A only) over 0.0 (center, both sides) to +1.0 (side B only). Values outside
     * are clamped by the engine. Crossfader zero is position 0.0, latch A/B is -1.0 / +1.0.
     */
    struct MixerSetCrossfader
    {
            float position;
    };

    /**
     * Momentary A/B: while held, the crossfader acts as if it were at the end of the side; on release (side None) it
     * goes back to its position. The position itself is not changed.
     */
    struct MixerCrossfaderMomentary
    {
            CrossfaderSide side;
    };

    /// Master volume, 0.0 (silent) to 1.0 (full). Values outside are clamped by the engine.
    struct MixerSetMasterVolume
    {
            float value;
    };

    /// Headphone cue button of a deck: the deck is heard on the headphones, taken before the channel fader
    struct MixerSetHeadphoneCue
    {
            DeckId deck;
            bool   enabled;
    };

    /// How loud a cued deck is on the headphones, 0.0 ... 1.0. Values outside are clamped by the engine.
    struct MixerSetHeadphoneLevel
    {
            DeckId deck;
            float  value;
    };

    /// Headphone mix: 0.0 only the cued decks, 1.0 only the master. Values outside are clamped by the engine.
    struct MixerSetCueMix
    {
            float value;
    };

    /// Headphone volume, 0.0 ... 1.0. Values outside are clamped by the engine.
    struct MixerSetHeadphoneVolume
    {
            float value;
    };

    /**
     * Everything a frontend or controller can ask the core to do.
     *
     * Commands are plain values without heap memory, so they can pass through a lock-free queue into the audio
     * thread. Keep it that way: a command that needs a string or a buffer (e.g. loading a file) is handled outside
     * the audio thread.
     */
    using Command =
        std::variant<DeckPlay, DeckPause, DeckCue, DeckSeek, DeckSetPitch, DeckPitchBend, MixerSetChannelVolume,
                     MixerAssignCrossfader, MixerSetCrossfader, MixerCrossfaderMomentary, MixerSetMasterVolume,
                     MixerSetHeadphoneCue, MixerSetHeadphoneLevel, MixerSetCueMix, MixerSetHeadphoneVolume>;

    static_assert(std::is_trivially_copyable_v<Command>);

    /// Queue from one producer (a frontend, the controller thread, ...) into the core.
    using CommandQueue = SpscQueue<Command, 256>;
} // namespace djw::core::api

#endif // DJW_CORE_API_COMMANDS_HPP
