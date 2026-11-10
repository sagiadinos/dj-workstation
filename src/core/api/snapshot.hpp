// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_API_SNAPSHOT_HPP
#define DJW_CORE_API_SNAPSHOT_HPP

#include "api/crossfader_side.hpp"
#include "api/deck_id.hpp"
#include "api/triple_buffer.hpp"

#include <cstdint>
#include <type_traits>

namespace djw::core::api
{
    enum class PlayState : std::uint8_t
    {
        Stopped,
        Playing
    };

    /**
     * Peak level of a stereo signal for the VU meters, linear: 1.0 is full scale (0 dBFS).
     *
     * Peaks are held by the engine and fall back by 20 dB in 1.7 s (like a PPM), so a frontend that reads less often
     * than the audio blocks come does not miss a peak.
     */
    struct StereoLevel
    {
            float left  = 0.0F;
            float right = 0.0F;
    };

    /**
     * State of one deck as the frontends see it.
     *
     * Published by the audio thread once per audio block. Every deck has its own snapshot buffer, so decks can be
     * added and removed without a fixed upper limit in the snapshot.
     */
    struct DeckSnapshot
    {
            DeckId    deck{};
            bool      track_loaded     = false;
            PlayState play_state       = PlayState::Stopped;
            float     channel_volume   = 1.0F;
            double    position_seconds = 0.0;
            double    duration_seconds = 0.0;
            double    cue_seconds      = 0.0;
            /// Goes up each time the cue point is set with the CUE button (not when a track is loaded), so a frontend
            /// can tell a cue point the user set from one that came with the track
            std::uint32_t cue_set_count = 0;
            /// Pitch fader, without a held pitch bend
            float          pitch_percent   = 0.0F;
            /// Keylock: the pitch fader changes the tempo only
            bool           keylock         = false;
            CrossfaderSide crossfader_side = CrossfaderSide::None;
            /// Level before the channel fader, so a deck can be watched with the fader down
            StereoLevel level{};
            bool        headphone_cue   = false;
            float       headphone_level = 1.0F;
    };

    static_assert(std::is_trivially_copyable_v<DeckSnapshot>);

    using DeckSnapshotBuffer = TripleBuffer<DeckSnapshot>;

    /// State of the mixer master section, published once per audio block
    struct MixerSnapshot
    {
            /// Crossfader fader position, -1.0 (A) ... +1.0 (B); a held momentary button does not change it
            float          crossfader_position  = 0.0F;
            CrossfaderSide crossfader_momentary = CrossfaderSide::None;
            float          master_volume        = 1.0F;
            /// Level of the master output, after volume and limiter
            StereoLevel master_level{};
            /// Current gain of the master limiter: 1.0 when it is not limiting
            float limiter_gain = 1.0F;
            /// False when the output device has fewer than 4 channels: master only, no headphones
            bool  headphones_available = false;
            float cue_mix              = 0.0F;
            float headphone_volume     = 1.0F;
    };

    static_assert(std::is_trivially_copyable_v<MixerSnapshot>);

    using MixerSnapshotBuffer = TripleBuffer<MixerSnapshot>;
} // namespace djw::core::api

#endif // DJW_CORE_API_SNAPSHOT_HPP
