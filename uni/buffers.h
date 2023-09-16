// This provides the barebones ref-counted buffer class that powers
// AnyArray and AnyString

#pragma once
#include "common.h"

namespace uni {
inline namespace buffers {

    struct alignas(8) SharedBufferHeader {
         // Number of typed elements this buffer can hold
        const uint32 capacity;
         // Ref count.  For uniquely owned buffers, this is always 1.
        mutable uint32 ref_count;
    };

    template <class T>
    struct SharableBuffer {
        static_assert(alignof(T) <= 8,
            "SharableBuffer with objects of align > 8 are not yet supported."
        );
        SharableBuffer () = delete;

        ALWAYS_INLINE
        static SharedBufferHeader* header (const T* data) {
            return (SharedBufferHeader*)data - 1;
        }

        static constexpr usize capacity_for_size (usize size) {
             // Working with malloc on glibc x64, it seems that malloc gives
             // sizes 24, 40, 56, 88, 104, ...
             // In other words, 8 + 16n.  And our header size just so happens to
             // be 8 bytes.  Nice!  However, malloc provide storage aligned to
             // 16-byte boundaries, so by inserting an 8-byte header we lose
             // this alignment.  It may be worth investigating if there's a
             // solution to this problem.

             // Give up on rounding up non-power-of-two sizes.
            usize mask = sizeof(T) == 1 ? 15
                       : sizeof(T) == 2 ? 7
                       : sizeof(T) == 4 ? 3
                       : sizeof(T) == 8 ? 1
                       : 0;
            return (size + mask) & ~mask;
        }

        [[gnu::malloc, gnu::returns_nonnull]] static
        T* allocate (usize size) {
            if (size > uint32(-1) >> 1) [[unlikely]] require(false);
            usize cap = capacity_for_size(size);
             // On 32-bit platforms we need to make sure we don't overflow usize
            uint64 bytes = sizeof(SharedBufferHeader) + (uint64)cap * sizeof(T);
            require(bytes <= usize(-1));
            auto header = (SharedBufferHeader*)std::malloc(bytes);
            const_cast<uint32&>(header->capacity) = cap;
            header->ref_count = 1;
            return (T*)(header + 1);
        }

        [[gnu::nonnull(1)]] ALWAYS_INLINE
        static void deallocate (T* buf) {
            std::free((SharedBufferHeader*)buf - 1);
        }
    };
}
}
