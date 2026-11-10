// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_ENGINE_DECK_HPP
#define DJW_CORE_ENGINE_DECK_HPP

#include "api/deck_id.hpp"
#include "engine/audio_source.hpp"
#include "engine/resampler.hpp"
#include "engine/time_stretcher.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace djw::core::engine
{
    /// Pitch fader range in percent
    inline constexpr float max_pitch_percent = 12.0F;
    /// Speed change while a pitch bend button is held, in percent
    inline constexpr double pitch_bend_percent = 4.0;

    /**
     * Transport of one deck: play, pause, CUE, seek and pitch of the loaded track.
     *
     * The track is played through a Resampler, so the pitch fader changes tempo and pitch together. With keylock on
     * it goes through a TimeStretcher instead: the pitch fader then changes the tempo only. Positions are in frames
     * of the track, independent of the pitch.
     *
     * Pause fades out over a few milliseconds instead of cutting, so it does not click; the same fade is used when
     * CUE stops the deck and returns to the cue point. During the fade the deck already reports itself as not
     * playing. At the end of the track the deck stops. CUE behaves like on a CDJ, see api::DeckCue.
     *
     * Volume and mixing are not part of the deck, they belong to its mixer channel.
     */
    class Deck
    {
        public:
            /// pause_fade_frames: length of the fade-out on pause, 0 stops at once. Allocates (the keylock buffers):
            /// create it outside the audio thread.
            Deck(api::DeckId id, std::size_t pause_fade_frames, std::uint32_t sample_rate = 48'000);

            /// Sets the track; a different track than before starts stopped at its beginning with the cue point
            /// there. nullptr unloads. The pitch stays, like a fader.
            void attach(AudioSource *track) noexcept; // [audio-thread]

            [[nodiscard]] api::DeckId   id() const noexcept { return deck_id; }
            [[nodiscard]] bool          hasTrack() const noexcept { return source != nullptr; }
            [[nodiscard]] bool          isPlaying() const noexcept { return state == State::Playing; }
            [[nodiscard]] double        positionFrames() const noexcept { return playbackPosition(); }
            [[nodiscard]] std::uint64_t cueFrame() const noexcept { return cue_frame; }
            [[nodiscard]] std::uint64_t lengthFrames() const noexcept;
            [[nodiscard]] float         pitchPercent() const noexcept { return pitch_percent; }
            [[nodiscard]] bool          keylock() const noexcept { return keylock_on; }
            /// Current playback speed, 1.0 = original
            [[nodiscard]] double speed() const noexcept;

            void play() noexcept;                      // [audio-thread]
            void pause() noexcept;                     // [audio-thread]
            void cue(bool pressed) noexcept;           // [audio-thread]
            void seek(std::uint64_t frame) noexcept;   // [audio-thread]
            void setPitch(float percent) noexcept;     // [audio-thread]
            void setPitchBend(int direction) noexcept; // [audio-thread]
            /// Keylock: the pitch fader and the bend buttons change the tempo only. The position stays.
            void setKeylock(bool on) noexcept; // [audio-thread]

            /// Fills output (interleaved stereo) with the next frames, silence while paused, without a track or when
            /// the audio at the position is not decoded yet.
            void render(std::span<float> output) noexcept; // [audio-thread]

        private:
            enum class State : std::uint8_t
            {
                Stopped,
                Playing,
                FadingOut
            };

            /// Position of the next frame, in frames of the track
            [[nodiscard]] double playbackPosition() const noexcept;
            /// Moves the track and both players to frame
            void restartAt(std::uint64_t frame) noexcept;
            /// Next frames of the track at the current speed, through the resampler or the time stretcher
            std::size_t produce(std::span<float> output) noexcept;
            [[nodiscard]] bool drained() const noexcept;

            /// Fades out, then stops at the cue point
            void               stopAtCue() noexcept; // [audio-thread]
            [[nodiscard]] bool atCue() const noexcept;

            std::size_t renderFadeOut(std::span<float> output) noexcept; // [audio-thread]

            api::DeckId   deck_id;
            AudioSource  *source = nullptr;
            Resampler     resampler;
            TimeStretcher stretcher;
            bool          keylock_on = false;
            std::size_t   pause_fade_frames;
            std::size_t   fade_remaining = 0;
            State         state          = State::Stopped;
            std::uint64_t cue_frame      = 0;
            bool          cue_preview    = false; ///< playing only while CUE is held
            bool          return_to_cue  = false; ///< jump to the cue point when the fade has ended
            float         pitch_percent  = 0.0F;
            int           bend_direction = 0;
    };
} // namespace djw::core::engine

#endif // DJW_CORE_ENGINE_DECK_HPP
