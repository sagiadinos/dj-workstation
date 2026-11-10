// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_API_SNAPSHOT_HPP
#define DJW_CORE_API_SNAPSHOT_HPP

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
            /// Pitch fader, without a held pitch bend
            float pitch_percent = 0.0F;
    };

    static_assert(std::is_trivially_copyable_v<DeckSnapshot>);

    using DeckSnapshotBuffer = TripleBuffer<DeckSnapshot>;
} // namespace djw::core::api

#endif // DJW_CORE_API_SNAPSHOT_HPP
