// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Niko Sagiadinos

#include "allocation_counter.hpp"

#include <cstdlib>
#include <new>

namespace
{
    // Constant-initialized, so touching them inside operator new does not allocate
    thread_local bool        counting      = false;
    thread_local std::size_t allocations   = 0;
    thread_local std::size_t deallocations = 0;

    void *allocate(std::size_t size)
    {
        if (counting)
        {
            ++allocations;
        }
        if (void *memory = std::malloc(size != 0 ? size : 1))
        {
            return memory;
        }
        throw std::bad_alloc{};
    }

    void *allocateAligned(std::size_t size, std::align_val_t alignment)
    {
        if (counting)
        {
            ++allocations;
        }
        const auto align   = static_cast<std::size_t>(alignment);
        const auto rounded = (size + align - 1) / align * align;
#ifdef _MSC_VER
        void *memory = _aligned_malloc(rounded != 0 ? rounded : align, align);
#else
        void *memory = std::aligned_alloc(align, rounded != 0 ? rounded : align);
#endif
        if (memory != nullptr)
        {
            return memory;
        }
        throw std::bad_alloc{};
    }

    void release(void *memory) noexcept
    {
        if (memory == nullptr)
        {
            return;
        }
        if (counting)
        {
            ++deallocations;
        }
        std::free(memory);
    }

    void releaseAligned(void *memory) noexcept
    {
        if (memory == nullptr)
        {
            return;
        }
        if (counting)
        {
            ++deallocations;
        }
#ifdef _MSC_VER
        _aligned_free(memory);
#else
        std::free(memory);
#endif
    }
} // namespace

namespace djw::tests
{
    AllocationCounter::AllocationCounter() noexcept
    {
        ::allocations   = 0;
        ::deallocations = 0;
        counting        = true;
    }

    AllocationCounter::~AllocationCounter()
    {
        counting = false;
    }

    std::size_t AllocationCounter::allocations() const noexcept
    {
        return ::allocations;
    }

    std::size_t AllocationCounter::deallocations() const noexcept
    {
        return ::deallocations;
    }
} // namespace djw::tests

// Replacements of the global allocation functions, see AllocationCounter
void *operator new(std::size_t size)
{
    return allocate(size);
}

void *operator new[](std::size_t size)
{
    return allocate(size);
}

void *operator new(std::size_t size, const std::nothrow_t &) noexcept
{
    try
    {
        return allocate(size);
    }
    catch (...)
    {
        return nullptr;
    }
}

void *operator new[](std::size_t size, const std::nothrow_t &) noexcept
{
    try
    {
        return allocate(size);
    }
    catch (...)
    {
        return nullptr;
    }
}

void *operator new(std::size_t size, std::align_val_t alignment)
{
    return allocateAligned(size, alignment);
}

void *operator new[](std::size_t size, std::align_val_t alignment)
{
    return allocateAligned(size, alignment);
}

void operator delete(void *memory) noexcept
{
    release(memory);
}

void operator delete[](void *memory) noexcept
{
    release(memory);
}

void operator delete(void *memory, std::size_t) noexcept
{
    release(memory);
}

void operator delete[](void *memory, std::size_t) noexcept
{
    release(memory);
}

void operator delete(void *memory, std::align_val_t) noexcept
{
    releaseAligned(memory);
}

void operator delete[](void *memory, std::align_val_t) noexcept
{
    releaseAligned(memory);
}

void operator delete(void *memory, std::size_t, std::align_val_t) noexcept
{
    releaseAligned(memory);
}

void operator delete[](void *memory, std::size_t, std::align_val_t) noexcept
{
    releaseAligned(memory);
}
