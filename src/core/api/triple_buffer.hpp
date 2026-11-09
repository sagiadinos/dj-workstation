// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_API_TRIPLE_BUFFER_HPP
#define DJW_CORE_API_TRIPLE_BUFFER_HPP

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace djw::core::api
{
    /**
     * Lock-free triple buffer for passing state from one writer thread to one reader thread.
     *
     * The writer (the audio thread) publishes a complete value, the reader (the UI) always gets the latest published
     * value. Neither side ever waits for the other: values the reader did not pick up in time are overwritten, which
     * is what state snapshots need. A value is never torn, the reader sees either the old or the new one.
     */
    template <typename T> class TripleBuffer
    {
            static_assert(std::is_trivially_copyable_v<T>, "copying a value must not allocate");

        public:
            TripleBuffer() = default;
            explicit TripleBuffer(const T &initial) noexcept : buffers{initial, initial, initial} {}

            /// Writer side: publishes value, replacing an unread older one.
            void write(const T &value) noexcept // [audio-thread]
            {
                buffers[write_index]        = value;
                const std::uint8_t previous = middle.exchange(write_index | dirty_bit, std::memory_order_acq_rel);
                write_index                 = previous & index_mask;
            }

            /// Reader side: returns the latest published value. The reference stays valid until the next read().
            const T &read() noexcept
            {
                if ((middle.load(std::memory_order_relaxed) & dirty_bit) != 0)
                {
                    const std::uint8_t previous = middle.exchange(read_index, std::memory_order_acq_rel);
                    read_index                  = previous & index_mask;
                }
                return buffers[read_index];
            }

        private:
            static constexpr std::size_t  cache_line = 64;
            static constexpr std::uint8_t index_mask = 0x3;
            // Set when the middle buffer holds a value the reader has not taken yet
            static constexpr std::uint8_t dirty_bit = 0x4;

            std::array<T, 3> buffers{};
            alignas(cache_line) std::uint8_t write_index = 0;
            alignas(cache_line) std::atomic<std::uint8_t> middle{1};
            alignas(cache_line) std::uint8_t read_index = 2;
    };
} // namespace djw::core::api

#endif // DJW_CORE_API_TRIPLE_BUFFER_HPP
