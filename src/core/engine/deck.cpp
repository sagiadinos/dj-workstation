// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "engine/deck.hpp"

#include <algorithm>

namespace djw::core::engine
{
    Deck::Deck(api::DeckId id, std::size_t pause_fade_frames) noexcept
        : deck_id{id}, pause_fade_frames{pause_fade_frames}
    {
    }

    void Deck::attach(AudioSource *track) noexcept // [audio-thread]
    {
        if (track == source)
        {
            return;
        }
        source          = track;
        state           = State::Stopped;
        fade_remaining  = 0;
        position_frames = 0;
    }

    std::uint64_t Deck::lengthFrames() const noexcept
    {
        return source != nullptr ? source->lengthFrames() : 0;
    }

    void Deck::play() noexcept // [audio-thread]
    {
        if (source != nullptr)
        {
            state = State::Playing;
        }
    }

    void Deck::pause() noexcept // [audio-thread]
    {
        if (state != State::Playing)
        {
            return;
        }
        state          = pause_fade_frames > 0 ? State::FadingOut : State::Stopped;
        fade_remaining = pause_fade_frames;
    }

    void Deck::seek(std::uint64_t frame) noexcept // [audio-thread]
    {
        if (source == nullptr)
        {
            return;
        }
        const std::uint64_t length = source->lengthFrames();
        position_frames            = length > 0 ? std::min(frame, length) : frame;
        source->seek(position_frames);
    }

    void Deck::render(std::span<float> output) noexcept // [audio-thread]
    {
        std::size_t frames_read = 0;
        if (state == State::Playing)
        {
            frames_read = source->read(output);
            if (frames_read * stereo_channels < output.size() && source->atEnd())
            {
                state = State::Stopped;
            }
        }
        else if (state == State::FadingOut)
        {
            frames_read = renderFadeOut(output);
        }
        position_frames += frames_read;
        std::ranges::fill(output.subspan(frames_read * stereo_channels), 0.0F);
    }

    std::size_t Deck::renderFadeOut(std::span<float> output) noexcept // [audio-thread]
    {
        const std::size_t wanted      = std::min(output.size() / stereo_channels, fade_remaining);
        const std::size_t frames_read = source->read(output.first(wanted * stereo_channels));

        // Linear from the current level down to 0.0, reached on the last frame of the fade
        for (std::size_t frame = 0; frame < frames_read; ++frame)
        {
            const float gain = static_cast<float>(fade_remaining - frame - 1) / static_cast<float>(pause_fade_frames);
            for (std::size_t channel = 0; channel < stereo_channels; ++channel)
            {
                output[frame * stereo_channels + channel] *= gain;
            }
        }

        fade_remaining -= wanted;
        if (fade_remaining == 0 || frames_read < wanted)
        {
            state = State::Stopped;
        }
        return frames_read;
    }
} // namespace djw::core::engine
