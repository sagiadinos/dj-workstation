// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_CORE_API_SPSC_QUEUE_HPP
#define DJW_CORE_API_SPSC_QUEUE_HPP

#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <optional>
#include <type_traits>

namespace djw::core::api
{
    /**
     * Bounded lock-free queue for exactly one producer thread and one consumer thread.
     *
     * Both sides are real-time safe: no allocation, no locks, bounded time. The storage is a fixed ring of Capacity
     * slots; a push into a full queue fails instead of blocking, so the producer decides what to do with it. Several
     * producers each get their own queue, which keeps the audio thread free of compare-and-swap loops.
     */
    template <typename T, std::size_t Capacity> class SpscQueue
    {
            static_assert(std::is_trivially_copyable_v<T>, "copying into a slot must not allocate");
            static_assert(Capacity >= 2 && std::has_single_bit(Capacity), "capacity must be a power of two");

        public:
            /// Producer side. Returns false if the queue is full.
            [[nodiscard]] bool tryPush(const T &value) noexcept // [audio-thread]
            {
                const std::size_t head_index = head.load(std::memory_order_relaxed);
                if (head_index - tail.load(std::memory_order_acquire) == Capacity)
                {
                    return false;
                }
                slots[head_index & index_mask] = value;
                head.store(head_index + 1, std::memory_order_release);
                return true;
            }

            /// Consumer side. Returns std::nullopt if the queue is empty.
            [[nodiscard]] std::optional<T> tryPop() noexcept // [audio-thread]
            {
                const std::size_t tail_index = tail.load(std::memory_order_relaxed);
                if (tail_index == head.load(std::memory_order_acquire))
                {
                    return std::nullopt;
                }
                T value = slots[tail_index & index_mask];
                tail.store(tail_index + 1, std::memory_order_release);
                return value;
            }

            static constexpr std::size_t capacity() noexcept { return Capacity; }

        private:
            // Fixed instead of std::hardware_destructive_interference_size, whose value may differ between
            // compilers and triggers -Winterference-size in headers.
            static constexpr std::size_t cache_line = 64;
            static constexpr std::size_t index_mask = Capacity - 1;

            // Head and tail only grow; their difference is the fill level, wrap-around of size_t is harmless.
            alignas(cache_line) std::atomic<std::size_t> head{0};
            alignas(cache_line) std::atomic<std::size_t> tail{0};
            alignas(cache_line) std::array<T, Capacity> slots{};
    };
} // namespace djw::core::api

#endif // DJW_CORE_API_SPSC_QUEUE_HPP
