// This provides the barebones ref-counted buffer class that powers
// AnyArray and AnyString

#pragma once
#include "common.h"

namespace uni {
inline namespace buffers {

    struct alignas(8) SharedBufferHeader {
         // Number of typed elements this buffer can hold
        const uint32 capacity;
         // zero-based ref count.  For uniquely owned buffers, this is always 0.
        mutable uint32 ref_count;
    };

    template <class T>
    struct SharedBuffer {
        static_assert(alignof(T) <= 8,
            "SharedBuffer with objects of align > 8 are not yet supported."
        );
        SharedBuffer () = delete;

        ALWAYS_INLINE
        static SharedBufferHeader* header (const T* data) {
            return (SharedBufferHeader*)data - 1;
        }

        static constexpr usize capacity_for_size (usize size) {
            usize min_bytes = sizeof(usize) == 8 ? 24 : 16;
            usize min_cap = min_bytes / sizeof(T);
            if (!min_cap) min_cap = 1;
             // Give up on rounding up non-power-of-two sizes.
            usize mask = sizeof(T) == 1 ? 7
                       : sizeof(T) == 2 ? 3
                       : sizeof(T) == 4 ? 1
                       : 0;
            return size <= min_cap ? min_cap : ((size + mask) & ~mask);
        }

        [[gnu::malloc, gnu::returns_nonnull]] static
        T* allocate (usize size) {
            require(size <= uint32(-1) >> 1);
            usize cap = capacity_for_size(size);
             // On 32-bit platforms we need to make sure we don't overflow usize
            uint64 bytes = sizeof(SharedBufferHeader) + (uint64)cap * sizeof(T);
            require(bytes <= usize(-1));
            auto header = (SharedBufferHeader*)std::malloc(bytes);
            const_cast<uint32&>(header->capacity) = cap;
            header->ref_count = 0;
            return (T*)(header + 1);
        }

        [[gnu::nonnull(1)]] ALWAYS_INLINE
        static void deallocate (T* buf) {
            std::free((SharedBufferHeader*)buf - 1);
        }
    };
}
}
