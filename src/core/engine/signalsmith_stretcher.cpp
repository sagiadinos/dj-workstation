// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "engine/time_stretcher.hpp"

#include <signalsmith-stretch/signalsmith-stretch.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace djw::core::engine
{
    namespace
    {
        /// Output frames made in one call of the library
        constexpr std::size_t step_frames = 512;

        /// Two channels of float, one after the other, as the library takes them (inputs[channel][frame])
        class Planar
        {
            public:
                explicit Planar(std::size_t frames) : left(frames), right(frames) {}

                [[nodiscard]] std::array<float *, stereo_channels> at(std::size_t offset) noexcept
                {
                    return {left.data() + offset, right.data() + offset};
                }
                [[nodiscard]] std::size_t capacity() const noexcept { return left.size(); }

                /// Takes frames of interleaved stereo in at offset
                void take(std::span<const float> interleaved, std::size_t offset) noexcept
                {
                    for (std::size_t frame = 0; frame < interleaved.size() / stereo_channels; ++frame)
                    {
                        left[offset + frame]  = interleaved[frame * stereo_channels];
                        right[offset + frame] = interleaved[(frame * stereo_channels) + 1];
                    }
                }
                /// Gives frames from offset out as interleaved stereo
                void give(std::span<float> interleaved, std::size_t offset) const noexcept
                {
                    for (std::size_t frame = 0; frame < interleaved.size() / stereo_channels; ++frame)
                    {
                        interleaved[frame * stereo_channels]       = left[offset + frame];
                        interleaved[(frame * stereo_channels) + 1] = right[offset + frame];
                    }
                }

            private:
                std::vector<float> left;
                std::vector<float> right;
        };

        /**
         * Keylock with Signalsmith Stretch (MIT): a phase vocoder that keeps tones clean, at a moderate cost.
         *
         * The library takes as much input per call as the tempo asks for and gives out what it made from input
         * inputLatency() frames back, plus outputLatency() frames in its output. After a reset it is primed with
         * outputSeek(), so the first frame out is the frame the deck stands at; at the end flush() gives what it
         * still holds.
         */
        class SignalsmithStretcher final : public TimeStretcher
        {
            public:
                explicit SignalsmithStretcher(std::uint32_t sample_rate)
                    : input{static_cast<std::size_t>(std::ceil(step_frames * max_tempo)) + 1}, output{step_frames},
                      priming{0}, tail{0}
                {
                    stretch.presetDefault(static_cast<int>(stereo_channels), static_cast<float>(sample_rate));
                    priming = Planar{static_cast<std::size_t>(stretch.outputSeekLength(max_tempo)) + 1};
                    // Everything held when the track ends, played out at the slowest tempo
                    tail = Planar{static_cast<std::size_t>(std::ceil(stretch.inputLatency() / min_tempo)) +
                                  static_cast<std::size_t>(stretch.outputLatency()) + 1};
                    warmUp();
                }

                void reset(std::uint64_t frame) noexcept override // [audio-thread]
                {
                    start       = static_cast<double>(frame);
                    fed         = 0;
                    primed      = false;
                    gathered    = 0;
                    debt        = 0.0;
                    ended       = false;
                    tail_length = 0;
                    tail_played = 0;
                }

                std::size_t process(AudioSource &source, double tempo,
                                    std::span<float> out) noexcept override // [audio-thread]
                {
                    tempo         = std::clamp(tempo, min_tempo, max_tempo);
                    current_tempo = tempo;
                    if (!primed && !prime(source, tempo))
                    {
                        return 0; // the source has not enough yet
                    }
                    const std::size_t wanted   = out.size() / stereo_channels;
                    std::size_t       produced = 0;
                    while (produced < wanted)
                    {
                        if (ended)
                        {
                            const std::size_t count = std::min(wanted - produced, tail_length - tail_played);
                            tail.give(out.subspan(produced * stereo_channels, count * stereo_channels), tail_played);
                            tail_played += count;
                            produced += count;
                            break;
                        }
                        const std::size_t made = step(source, tempo, std::min(step_frames, wanted - produced));
                        output.give(out.subspan(produced * stereo_channels, made * stereo_channels), 0);
                        produced += made;
                        if (made == 0 && !ended)
                        {
                            break; // not decoded yet
                        }
                    }
                    return produced;
                }

                [[nodiscard]] double position() const noexcept override // [audio-thread]
                {
                    if (!primed)
                    {
                        return start;
                    }
                    // Fed, minus what the library holds: its input latency and its output latency at the tempo; at
                    // the end what the tail still has to play
                    const double held = ended ? static_cast<double>(tail_length - tail_played) * current_tempo
                                              : stretch.inputLatency() + (stretch.outputLatency() * current_tempo);
                    return start + std::max(0.0, fed - held);
                }

                [[nodiscard]] bool drained() const noexcept override // [audio-thread]
                {
                    return ended && tail_played == tail_length;
                }

            private:
                /// Reads up to frames from the source into the planar buffer at offset; the frames read
                static std::size_t read(AudioSource &source, Planar &into, std::size_t offset, std::size_t frames,
                                        std::span<float> scratch) noexcept
                {
                    std::size_t done = 0;
                    while (done < frames)
                    {
                        const std::size_t count = std::min(frames - done, scratch.size() / stereo_channels);
                        const std::size_t got   = source.read(scratch.first(count * stereo_channels));
                        into.take(std::span<const float>{scratch}.first(got * stereo_channels), offset + done);
                        done += got;
                        if (got < count)
                        {
                            break;
                        }
                    }
                    return done;
                }

                /// After a reset: gathers outputSeekLength() frames and primes the library with them, so the next
                /// frame out is the first one read. False while the source has not enough yet.
                bool prime(AudioSource &source, double tempo) noexcept
                {
                    const auto needed = static_cast<std::size_t>(stretch.outputSeekLength(static_cast<float>(tempo)));
                    gathered += read(source, priming, gathered, needed - std::min(needed, gathered), scratch);
                    if (gathered < needed && !source.atEnd())
                    {
                        return false;
                    }
                    auto channels = priming.at(0);
                    stretch.outputSeek(channels, static_cast<int>(gathered));
                    fed    = static_cast<double>(gathered);
                    primed = true;
                    return true;
                }

                /// Makes up to frames of output into the output buffer; at the end of the source it makes the tail
                std::size_t step(AudioSource &source, double tempo, std::size_t frames) noexcept
                {
                    debt += static_cast<double>(frames) * tempo;
                    const auto  wanted = static_cast<std::size_t>(debt);
                    const auto  got    = read(source, input, 0, wanted, scratch);
                    std::size_t made   = frames;
                    if (got < wanted)
                    {
                        // The end, or not decoded yet: as much output as the input there is gives
                        made = static_cast<std::size_t>(static_cast<double>(got) / tempo);
                        debt = std::max(0.0, static_cast<double>(got) - (static_cast<double>(made) * tempo));
                    }
                    else
                    {
                        debt -= static_cast<double>(wanted);
                    }
                    auto in  = input.at(0);
                    auto out = output.at(0);
                    stretch.process(in, static_cast<int>(got), out, static_cast<int>(made));
                    fed += static_cast<double>(got);
                    if (got < wanted && source.atEnd())
                    {
                        finish(tempo);
                    }
                    return made;
                }

                /// The source ended: what the library holds goes into the tail
                void finish(double tempo) noexcept
                {
                    const double held = stretch.inputLatency() + (stretch.outputLatency() * tempo);
                    tail_length       = std::min(tail.capacity(), static_cast<std::size_t>(held / tempo));
                    auto channels     = tail.at(0);
                    stretch.flush(channels, static_cast<int>(tail_length), static_cast<float>(tempo));
                    tail_played = 0;
                    ended       = true;
                }

                /// Runs silence through at the slowest and fastest tempo and flushes, so every buffer has its size
                void warmUp()
                {
                    struct Silence final : AudioSource
                    {
                            std::size_t read(std::span<float> into) noexcept override
                            {
                                std::ranges::fill(into, 0.0F);
                                return into.size() / stereo_channels;
                            }
                            void                        seek(std::uint64_t /*frame*/) noexcept override {}
                            [[nodiscard]] std::uint64_t lengthFrames() const noexcept override { return 0; }
                            [[nodiscard]] bool          atEnd() const noexcept override { return false; }
                    } silence;
                    std::vector<float> sink(step_frames * stereo_channels * 4);
                    for (const double tempo : {min_tempo, max_tempo, 1.0})
                    {
                        reset(0);
                        for (int block = 0; block < 8; ++block)
                        {
                            static_cast<void>(process(silence, tempo, sink));
                        }
                        finish(tempo);
                    }
                    reset(0);
                }

                signalsmith::stretch::SignalsmithStretch<float> stretch;
                Planar                                          input;
                Planar                                          output;
                Planar                                          priming;
                Planar                                          tail;
                std::array<float, 256 * stereo_channels>        scratch{};
                double                                          current_tempo = 1.0;
                double                                          start         = 0; ///< source frame at the last reset
                double                                          fed           = 0; ///< frames given since
                double                                          debt          = 0; ///< input owed for output made
                std::size_t                                     gathered      = 0; ///< frames read for priming
                std::size_t                                     tail_length   = 0;
                std::size_t                                     tail_played   = 0;
                bool                                            primed        = false;
                bool                                            ended         = false;
        };
    } // namespace

    std::unique_ptr<TimeStretcher> makeSignalsmithStretcher(std::uint32_t sample_rate)
    {
        return std::make_unique<SignalsmithStretcher>(sample_rate);
    }
} // namespace djw::core::engine
