// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "engine/deck.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace djw::core::engine
{
    Deck::Deck(api::DeckId id, std::size_t pause_fade_frames, std::uint32_t sample_rate)
        : deck_id{id}, stretcher{sample_rate}, pause_fade_frames{pause_fade_frames},
          scrub_loop_frames{static_cast<std::size_t>(scrub_loop_seconds * sample_rate)},
          scrub_ramp_frames{std::max<std::size_t>(1, static_cast<std::size_t>(scrub_ramp_seconds * sample_rate))}
    {
    }

    void Deck::attach(AudioSource *track) noexcept // [audio-thread]
    {
        if (track == source)
        {
            return;
        }
        source         = track;
        state          = State::Stopped;
        resume_playing = false;
        fade_remaining = 0;
        cue_frame      = 0;
        cue_preview    = false;
        return_to_cue  = false;
        resampler.reset(0);
        stretcher.reset(0);
        if (source != nullptr)
        {
            // The track says where to start: the first sound or the cue point stored for it; this is the cue point
            const std::uint64_t length = source->lengthFrames();
            const std::uint64_t start  = source->startFrame();
            if (start > 0 && (length == 0 || start < length))
            {
                cue_frame = start;
                restartAt(start);
            }
        }
    }

    double Deck::playbackPosition() const noexcept
    {
        if (state == State::Scrubbing)
        {
            return static_cast<double>(scrub_target);
        }
        return keylock_on ? stretcher.position() : resampler.position();
    }

    void Deck::restartAt(std::uint64_t frame) noexcept
    {
        source->seek(frame);
        resampler.reset(frame);
        stretcher.reset(frame);
    }

    std::size_t Deck::produce(std::span<float> output) noexcept
    {
        return keylock_on ? stretcher.process(*source, speed(), output) : resampler.process(*source, speed(), output);
    }

    bool Deck::drained() const noexcept
    {
        return keylock_on ? stretcher.drained() : resampler.drained();
    }

    std::uint64_t Deck::lengthFrames() const noexcept
    {
        return source != nullptr ? source->lengthFrames() : 0;
    }

    double Deck::speed() const noexcept
    {
        return (1.0 + pitch_percent / 100.0) * (1.0 + bend_direction * pitch_bend_percent / 100.0);
    }

    void Deck::play() noexcept // [audio-thread]
    {
        if (source == nullptr || scrubOfPlayingDeck())
        {
            return;
        }
        if (state == State::Scrubbing)
        {
            // The loop of a deck that stood ends, the deck plays from there
            restartAt(scrub_target);
        }
        // Play while CUE is held: the deck keeps playing after CUE is released
        cue_preview   = false;
        return_to_cue = false;
        state         = State::Playing;
    }

    void Deck::pause() noexcept // [audio-thread]
    {
        if (state == State::Scrubbing)
        {
            return;
        }
        cue_preview = false;
        if (state != State::Playing)
        {
            return;
        }
        state          = pause_fade_frames > 0 ? State::FadingOut : State::Stopped;
        fade_remaining = pause_fade_frames;
    }

    void Deck::cue(bool pressed) noexcept // [audio-thread]
    {
        if (source == nullptr || scrubOfPlayingDeck())
        {
            return;
        }
        if (state == State::Scrubbing)
        {
            // The loop of a deck that stood ends; where it played becomes the cue point
            if (pressed)
            {
                state     = State::Stopped;
                cue_frame = scrub_target;
                ++cue_sets;
                restartAt(scrub_target);
            }
            return;
        }
        if (!pressed)
        {
            if (cue_preview)
            {
                cue_preview = false;
                stopAtCue();
            }
            return;
        }
        if (state != State::Stopped)
        {
            stopAtCue();
        }
        else if (!atCue())
        {
            cue_frame = static_cast<std::uint64_t>(std::llround(playbackPosition()));
            ++cue_sets;
        }
        else
        {
            cue_preview = true;
            state       = State::Playing;
        }
    }

    void Deck::stopAtCue() noexcept // [audio-thread]
    {
        if (state == State::Playing && pause_fade_frames > 0)
        {
            state          = State::FadingOut;
            fade_remaining = pause_fade_frames;
            return_to_cue  = true;
            return;
        }
        if (state == State::FadingOut)
        {
            return_to_cue = true;
            return;
        }
        state = State::Stopped;
        seek(cue_frame);
    }

    bool Deck::atCue() const noexcept
    {
        return std::llround(playbackPosition()) == static_cast<long long>(cue_frame);
    }

    void Deck::seek(std::uint64_t frame) noexcept // [audio-thread]
    {
        if (source == nullptr)
        {
            return;
        }
        if (state == State::Scrubbing)
        {
            // The loop moves there
            scrub(frame, true);
            return;
        }
        restartAt(clampToTrack(frame));
    }

    void Deck::scrub(std::uint64_t frame, bool active) noexcept // [audio-thread]
    {
        if (source == nullptr)
        {
            return;
        }
        const std::uint64_t target = clampToTrack(frame);
        if (active)
        {
            if (state != State::Scrubbing)
            {
                // A deck fading out to a stop counts as stopped; the scrub starts from what it was doing
                resume_playing = state == State::Playing;
                cue_preview    = false;
                return_to_cue  = false;
                fade_remaining = 0;
                state          = State::Scrubbing;
                scrub_offset   = scrub_loop_frames; // the loop starts at once
                scrub_moved    = true;
            }
            if (target != scrub_target)
            {
                scrub_moved = true; // the loop moves there at once
            }
            scrub_target = target;
            return;
        }
        if (state != State::Scrubbing)
        {
            seek(target);
            return;
        }
        state = resume_playing ? State::Playing : State::Stopped;
        restartAt(target);
        resume_playing = false;
    }

    void Deck::setPitch(float percent) noexcept // [audio-thread]
    {
        pitch_percent = std::clamp(percent, -max_pitch_percent, max_pitch_percent);
    }

    void Deck::setPitchBend(int direction) noexcept // [audio-thread]
    {
        bend_direction = std::clamp(direction, -1, 1);
    }

    void Deck::setKeylock(bool on) noexcept // [audio-thread]
    {
        if (on == keylock_on)
        {
            return;
        }
        if (source == nullptr)
        {
            keylock_on = on;
            return;
        }
        // The other player takes over from where this one is
        const auto frame = static_cast<std::uint64_t>(std::llround(playbackPosition()));
        keylock_on       = on;
        restartAt(clampToTrack(frame));
    }

    std::uint64_t Deck::clampToTrack(std::uint64_t frame) const noexcept // [audio-thread]
    {
        const std::uint64_t length = source->lengthFrames();
        return length > 0 ? std::min(frame, length) : frame;
    }

    void Deck::render(std::span<float> output) noexcept // [audio-thread]
    {
        std::size_t produced = 0;
        if (state == State::Playing)
        {
            produced = produce(output);
            if (produced * stereo_channels < output.size() && drained())
            {
                state       = State::Stopped;
                cue_preview = false;
            }
        }
        else if (state == State::FadingOut)
        {
            produced = renderFadeOut(output);
        }
        else if (state == State::Scrubbing)
        {
            produced = renderScrub(output);
        }
        std::ranges::fill(output.subspan(produced * stereo_channels), 0.0F);
    }

    std::size_t Deck::renderScrub(std::span<float> output) noexcept // [audio-thread]
    {
        const std::size_t wanted  = output.size() / stereo_channels;
        std::size_t       written = 0;
        while (written < wanted)
        {
            if (scrub_moved || scrub_offset >= scrub_loop_frames)
            {
                source->seek(scrub_target);
                scrub_offset = 0;
                scrub_moved  = false;
            }
            const std::size_t room = std::min(wanted - written, scrub_loop_frames - scrub_offset);
            const std::size_t read = source->read(output.subspan(written * stereo_channels, room * stereo_channels));
            if (read == 0)
            {
                // Nothing there (not decoded yet, or the end): silence, and try the position again next time
                scrub_offset = scrub_loop_frames;
                break;
            }
            // Fade in at the start of the loop and out at its end, so the repeats do not click
            for (std::size_t frame = 0; frame < read; ++frame)
            {
                const std::size_t at = scrub_offset + frame;
                const float       in = std::min(1.0F, static_cast<float>(at) / static_cast<float>(scrub_ramp_frames));
                const float       out =
                    std::min(1.0F, static_cast<float>(scrub_loop_frames - at) / static_cast<float>(scrub_ramp_frames));
                const float gain = std::min(in, out);
                for (std::size_t channel = 0; channel < stereo_channels; ++channel)
                {
                    output[((written + frame) * stereo_channels) + channel] *= gain;
                }
            }
            scrub_offset += read;
            written += read;
            if (read < room)
            {
                scrub_offset = scrub_loop_frames;
                break;
            }
        }
        return written;
    }

    std::size_t Deck::renderFadeOut(std::span<float> output) noexcept // [audio-thread]
    {
        const std::size_t wanted   = std::min(output.size() / stereo_channels, fade_remaining);
        const std::size_t produced = produce(output.first(wanted * stereo_channels));

        // Linear from the current level down to 0.0, reached on the last frame of the fade
        for (std::size_t frame = 0; frame < produced; ++frame)
        {
            const float gain = static_cast<float>(fade_remaining - frame - 1) / static_cast<float>(pause_fade_frames);
            for (std::size_t channel = 0; channel < stereo_channels; ++channel)
            {
                output[frame * stereo_channels + channel] *= gain;
            }
        }

        fade_remaining -= wanted;
        if (fade_remaining == 0 || produced < wanted)
        {
            state = State::Stopped;
            if (std::exchange(return_to_cue, false))
            {
                seek(cue_frame);
            }
        }
        return produced;
    }
} // namespace djw::core::engine
