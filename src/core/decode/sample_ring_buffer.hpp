// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_DECODE_SAMPLE_RING_BUFFER_HPP
#define DJW_CORE_DECODE_SAMPLE_RING_BUFFER_HPP

#include <atomic>
#include <cstddef>
#include <span>
#include <vector>

namespace djw::core::decode
{
    /**
     * Lock-free ring buffer of float samples between one writer (decoder thread) and one reader (audio thread).
     *
     * The memory is allocated once in the constructor. Reads and writes copy as much as fits and return the count,
     * they never wait. Unlike SpscQueue it moves blocks of samples, not single values.
     */
    class SampleRingBuffer
    {
        public:
            /// The capacity is rounded up to a power of two.
            explicit SampleRingBuffer(std::size_t min_capacity);

            std::size_t capacity() const noexcept { return buffer.size(); }

            /// Writer side: samples that can be written now
            std::size_t writeAvailable() const noexcept;
            /// Writer side: copies as many samples as fit, returns how many
            std::size_t write(std::span<const float> samples) noexcept;

            /// Reader side: samples that can be read now
            std::size_t readAvailable() const noexcept; // [audio-thread]
            /// Reader side: copies up to samples.size() samples, returns how many
            std::size_t read(std::span<float> samples) noexcept; // [audio-thread]

        private:
            static constexpr std::size_t cache_line = 64;

            std::vector<float> buffer;
            std::size_t        index_mask;
            // Both only grow, their difference is the fill level
            alignas(cache_line) std::atomic<std::size_t> head{0};
            alignas(cache_line) std::atomic<std::size_t> tail{0};
    };
} // namespace djw::core::decode

#endif // DJW_CORE_DECODE_SAMPLE_RING_BUFFER_HPP
