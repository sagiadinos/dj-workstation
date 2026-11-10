// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_DECODE_STREAMING_SOURCE_HPP
#define DJW_CORE_DECODE_STREAMING_SOURCE_HPP

#include "decode/decoder.hpp"
#include "decode/sample_ring_buffer.hpp"
#include "engine/audio_source.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

namespace djw::core::decode
{
    /**
     * Track that is streamed from disk through a ring buffer; used for tracks too long to hold in memory.
     *
     * A worker thread keeps the ring buffer filled, the audio thread only copies out of it. The constructor already
     * fills the buffer on the calling thread, so playback can start right away. If the worker falls behind, read()
     * returns fewer frames and the deck plays silence until the buffer has data again.
     *
     * Seek: the audio thread hands the target to the worker and drops everything buffered. The worker moves the
     * decoder and tells the audio thread how much of the ring was written before, so no old audio slips through.
     * Until new audio arrives read() returns nothing, which is the short gap of a seek in a streamed track.
     */
    class StreamingSource final : public engine::AudioSource
    {
        public:
            StreamingSource(std::unique_ptr<Decoder> decoder, std::size_t buffer_frames);
            StreamingSource(const StreamingSource &)            = delete;
            StreamingSource &operator=(const StreamingSource &) = delete;
            StreamingSource(StreamingSource &&)                 = delete;
            StreamingSource &operator=(StreamingSource &&)      = delete;
            ~StreamingSource() override;

            std::size_t                 read(std::span<float> output) noexcept override; // [audio-thread]
            void                        seek(std::uint64_t frame) noexcept override;     // [audio-thread]
            [[nodiscard]] std::uint64_t lengthFrames() const noexcept override { return length; }
            [[nodiscard]] bool          atEnd() const noexcept override; // [audio-thread]

            /// The decoder reported an error; the stream ends at that point.
            [[nodiscard]] bool failed() const noexcept { return decode_failed.load(std::memory_order_acquire); }

        private:
            /// Worker: moves the decoder if the audio thread asked for a seek
            void handleSeek();
            /// Worker: decodes one chunk if there is room for it. Returns false if there was nothing to do.
            bool fillChunk();
            void run();

            /// Audio thread: the worker has handled the latest seek and the old audio is dropped
            bool seekSettled() noexcept; // [audio-thread]

            std::unique_ptr<Decoder> decoder;
            std::uint64_t            length;
            SampleRingBuffer         ring;
            std::vector<float>       chunk;
            std::atomic<bool>        decoder_done{false};
            std::atomic<bool>        decode_failed{false};
            std::atomic<bool>        stop_requested{false};

            // Seek handshake. The audio thread writes target and generation, the worker answers with the generation
            // and the ring position up to which the buffered audio is from before the seek.
            std::atomic<std::uint64_t> seek_frame{0};
            std::atomic<std::uint32_t> seek_generation{0};
            std::atomic<std::uint32_t> answered_generation{0};
            std::atomic<std::size_t>   stale_until{0};
            std::uint32_t              requested_generation = 0; // audio thread
            std::uint32_t              handled_generation   = 0; // worker

            // std::thread instead of std::jthread: jthread is missing or experimental in the minimum standard
            // libraries (libc++ 17, Apple Clang 15). The destructor stops and joins it.
            std::thread worker;
    };
} // namespace djw::core::decode

#endif // DJW_CORE_DECODE_STREAMING_SOURCE_HPP
