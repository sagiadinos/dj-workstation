// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#ifndef DJW_TESTS_SUPPORT_ALLOCATION_COUNTER_HPP
#define DJW_TESTS_SUPPORT_ALLOCATION_COUNTER_HPP

#include <cstddef>

namespace djw::tests
{
    /**
     * Counts heap allocations and deallocations of the current thread while it exists.
     *
     * Linking this file replaces the global operator new/delete of the test executable. Used to prove that code on
     * the audio thread does not touch the heap. Only C++ allocations are seen, malloc from C libraries is not.
     */
    class AllocationCounter
    {
        public:
            AllocationCounter() noexcept;
            AllocationCounter(const AllocationCounter &)            = delete;
            AllocationCounter &operator=(const AllocationCounter &) = delete;
            AllocationCounter(AllocationCounter &&)                 = delete;
            AllocationCounter &operator=(AllocationCounter &&)      = delete;
            ~AllocationCounter();

            std::size_t allocations() const noexcept;
            std::size_t deallocations() const noexcept;
    };
} // namespace djw::tests

#endif // DJW_TESTS_SUPPORT_ALLOCATION_COUNTER_HPP
