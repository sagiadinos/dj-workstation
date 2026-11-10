// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "decode/sample_ring_buffer.hpp"

#include <algorithm>
#include <bit>

namespace djw::core::decode
{
    SampleRingBuffer::SampleRingBuffer(std::size_t min_capacity)
        : buffer(std::bit_ceil(std::max<std::size_t>(min_capacity, 2))), index_mask{buffer.size() - 1}
    {
    }

    std::size_t SampleRingBuffer::writeAvailable() const noexcept
    {
        return buffer.size() - (head.load(std::memory_order_relaxed) - tail.load(std::memory_order_acquire));
    }

    std::size_t SampleRingBuffer::write(std::span<const float> samples) noexcept
    {
        const std::size_t head_index = head.load(std::memory_order_relaxed);
        const std::size_t count      = std::min(samples.size(), writeAvailable());
        const std::size_t start      = head_index & index_mask;
        const std::size_t first_part = std::min(count, buffer.size() - start);

        std::copy_n(samples.begin(), first_part, buffer.begin() + static_cast<std::ptrdiff_t>(start));
        std::copy_n(samples.begin() + static_cast<std::ptrdiff_t>(first_part), count - first_part, buffer.begin());
        head.store(head_index + count, std::memory_order_release);
        return count;
    }

    std::size_t SampleRingBuffer::readAvailable() const noexcept // [audio-thread]
    {
        return head.load(std::memory_order_acquire) - tail.load(std::memory_order_relaxed);
    }

    std::size_t SampleRingBuffer::read(std::span<float> samples) noexcept // [audio-thread]
    {
        const std::size_t tail_index = tail.load(std::memory_order_relaxed);
        const std::size_t count      = std::min(samples.size(), readAvailable());
        const std::size_t start      = tail_index & index_mask;
        const std::size_t first_part = std::min(count, buffer.size() - start);

        std::copy_n(buffer.begin() + static_cast<std::ptrdiff_t>(start), first_part, samples.begin());
        std::copy_n(buffer.begin(), count - first_part, samples.begin() + static_cast<std::ptrdiff_t>(first_part));
        tail.store(tail_index + count, std::memory_order_release);
        return count;
    }

    std::size_t SampleRingBuffer::discard(std::size_t count) noexcept // [audio-thread]
    {
        const std::size_t dropped = std::min(count, readAvailable());
        tail.store(tail.load(std::memory_order_relaxed) + dropped, std::memory_order_release);
        return dropped;
    }
} // namespace djw::core::decode
