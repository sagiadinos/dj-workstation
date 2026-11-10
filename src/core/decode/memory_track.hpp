// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_DECODE_MEMORY_TRACK_HPP
#define DJW_CORE_DECODE_MEMORY_TRACK_HPP

#include "decode/decoder.hpp"
#include "engine/audio_source.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <thread>
#include <vector>

namespace djw::core::decode
{
    /**
     * Track decoded completely into memory as interleaved stereo float32, see Architecture.md (Track loading).
     *
     * A worker thread decodes the track in chunks from start to end. The first chunk is decoded by create(), so
     * playback can start at once. Every position that is decoded can be read and jumped to without delay. A seek
     * into a chunk that is not decoded yet makes the worker decode that chunk next and continue from there; until
     * then read() returns nothing. Decoded chunks never change, the audio thread reads them without locks.
     */
    class MemoryTrack final : public engine::AudioSource
    {
        public:
            /// About 1.4 s at 48 kHz
            static constexpr std::size_t default_chunk_frames = 65'536;

            /// Allocates memory for length_frames and decodes the first chunk. If the memory cannot be allocated, the
            /// decoder is handed back, so the caller can stream the track instead.
            static std::expected<std::unique_ptr<MemoryTrack>, std::unique_ptr<Decoder>>
            create(std::unique_ptr<Decoder> decoder, std::uint64_t length_frames,
                   std::size_t chunk_frames = default_chunk_frames);

            MemoryTrack(const MemoryTrack &)            = delete;
            MemoryTrack &operator=(const MemoryTrack &) = delete;
            MemoryTrack(MemoryTrack &&)                 = delete;
            MemoryTrack &operator=(MemoryTrack &&)      = delete;
            ~MemoryTrack() override;

            std::size_t                 read(std::span<float> output) noexcept override; // [audio-thread]
            void                        seek(std::uint64_t frame) noexcept override;     // [audio-thread]
            [[nodiscard]] std::uint64_t lengthFrames() const noexcept override { return length; }
            [[nodiscard]] bool          atEnd() const noexcept override { return read_position >= length; }

            /// The audio at frame is decoded and can be read
            [[nodiscard]] bool isDecoded(std::uint64_t frame) const noexcept;
            /// The whole track is decoded
            [[nodiscard]] bool fullyDecoded() const noexcept;
            /// The decoder reported an error; the rest of the track is silence.
            [[nodiscard]] bool failed() const noexcept { return decode_failed.load(std::memory_order_acquire); }

        private:
            static constexpr std::size_t no_chunk = static_cast<std::size_t>(-1);

            struct Layout
            {
                    std::uint64_t length_frames;
                    std::size_t   chunk_frames;
            };

            MemoryTrack(std::unique_ptr<Decoder> decoder, Layout layout, std::vector<float> samples);

            [[nodiscard]] std::size_t chunkCount() const noexcept;
            [[nodiscard]] std::size_t chunkLength(std::size_t chunk) const noexcept;

            /// Worker: the chunk to decode next, no_chunk when everything is decoded
            std::size_t nextChunk() noexcept;
            void        decodeChunk(std::size_t chunk);
            void        run();

            std::unique_ptr<Decoder>       decoder;
            std::uint64_t                  length;
            std::size_t                    chunk_frames;
            std::vector<float>             samples;
            std::vector<std::atomic<bool>> ready;
            /// Set by the audio thread when it needs a chunk that is not decoded yet
            std::atomic<std::size_t> wanted_chunk{no_chunk};
            std::atomic<bool>        decode_failed{false};
            std::atomic<bool>        stop_requested{false};

            std::uint64_t read_position = 0; // audio thread
            std::size_t   decoder_chunk = 0; // worker: the chunk the decoder is positioned at
            std::size_t   first_missing = 0; // worker: no chunk before this one is missing

            // std::thread instead of std::jthread, see StreamingSource
            std::thread worker;
    };
} // namespace djw::core::decode

#endif // DJW_CORE_DECODE_MEMORY_TRACK_HPP
