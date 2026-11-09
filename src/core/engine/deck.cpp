// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "engine/deck.hpp"

#include <algorithm>

namespace djw::core::engine
{
    Deck::Deck(api::DeckId id, AudioSource &source) noexcept : deck_id{id}, source{&source} {}

    void Deck::render(std::span<float> output) noexcept // [audio-thread]
    {
        std::size_t frames_read = 0;
        if (playing)
        {
            frames_read = source->read(output);
            position_frames += frames_read;
        }
        std::ranges::fill(output.subspan(frames_read * stereo_channels), 0.0F);
    }
} // namespace djw::core::engine
