// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_DECODE_STREAMING_SOURCE_HPP
#define DJW_CORE_DECODE_STREAMING_SOURCE_HPP

#include "decode/decoder.hpp"
#include "decode/sample_ring_buffer.hpp"
#include "engine/audio_source.hpp"

#include <atomic>
#include <cstddef>
#include <memory>
#include <stop_token>
#include <thread>
#include <vector>

namespace djw::core::decode
{
    /**
     * Audio source that streams a decoder through a ring buffer.
     *
     * A worker thread keeps the ring buffer filled, the audio thread only copies out of it. The constructor already
     * fills the buffer on the calling thread, so playback can start right away. If the worker falls behind, read()
     * returns fewer frames and the deck plays silence until the buffer has data again.
     */
    class StreamingSource final : public engine::AudioSource
    {
        public:
            StreamingSource(std::unique_ptr<Decoder> decoder, std::size_t buffer_frames);
            ~StreamingSource() override;

            std::size_t read(std::span<float> output) noexcept override; // [audio-thread]

            /// The decoder reached the end of the stream (or failed) and everything decoded has been read.
            bool endOfStream() const noexcept;
            /// The decoder reported an error; the stream ends at that point.
            bool failed() const noexcept { return decode_failed.load(std::memory_order_acquire); }

        private:
            /// Decodes one chunk if there is room for it. Returns false if there was nothing to do.
            bool fillChunk();
            void run(const std::stop_token &stop);

            std::unique_ptr<Decoder> decoder;
            SampleRingBuffer         ring;
            std::vector<float>       chunk;
            std::atomic<bool>        decoder_done{false};
            std::atomic<bool>        decode_failed{false};
            // Declared last: destroyed first, so the worker is stopped and joined before the members it uses go away.
            std::jthread worker;
    };
} // namespace djw::core::decode

#endif // DJW_CORE_DECODE_STREAMING_SOURCE_HPP
