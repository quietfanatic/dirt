// This provides the barebones ref-counted buffer class that powers
// AnyArray and AnyString.

#pragma once
#include "common.h"
#include "lilac.h"

namespace uni {
inline namespace buffers {

    struct alignas(8) SharableBufferHeader {
         // Number of typed elements this buffer can hold
        const uint32 capacity;
         // Ref count.  For uniquely owned buffers, this is always 1.
        mutable uint32 ref_count;
    };

    template <class T>
    struct SharableBuffer {
        static_assert(alignof(T) <= 8,
            "SharableBuffer with elements of align > 8 are not yet supported."
        );
        SharableBuffer () = delete;

        ALWAYS_INLINE static
        SharableBufferHeader* header (const T* data) {
            return (SharableBufferHeader*)data - 1;
        }

        static constexpr usize min_capacity = 8 / sizeof(T) ? 8 / sizeof(T) : 1;
         // Match max_capacity with ArrayInterface's max_size
        static constexpr usize max_capacity = 0x7fffffff;

         // Round up the requested size to a power of two, anticipating
         // continual growth.
        static constexpr usize plenty_for_size (usize size) {
             // Pick a reasonable first capacity for various sizes of objects.
             // There isn't a whole lot of science to these choices, besides
             // that AYU tends to have a lot of arrays of size 2*16 bytes.
            constexpr usize min = sizeof(T) <= 1 ? 16
                                : sizeof(T) <= 2 ? 8
                                : sizeof(T) <= 8 ? 4
                                : sizeof(T) <= 64 ? 2
                                : 1;
            if (size > 0x40000000) [[unlikely]] {
                require(size <= max_capacity);
                return max_capacity;
            }
            else if (size <= min) return min;
            else {
                 // This should be fast on any modern processor
                return std::bit_ceil(uint32(size));
            }
        }

        [[gnu::malloc, gnu::returns_nonnull]] ALWAYS_INLINE static
        T* allocate (usize size) {
             // Use uint64 instead of usize because on 32-bit platforms we need
             // to make sure we don't overflow usize.
            uint64 bytes = sizeof(SharableBufferHeader) + (uint64)size * sizeof(T);
            require(bytes <= usize(-1));
            auto block = lilac::allocate_block(bytes);
            auto header = (SharableBufferHeader*)block.address;
            const_cast<uint32&>(header->capacity) =
                (block.capacity - sizeof(SharableBufferHeader)) / sizeof(T);
            header->ref_count = 1;
            return (T*)(header + 1);
        }

         // Allocate rounding up to a power of two
        [[gnu::malloc, gnu::returns_nonnull]] ALWAYS_INLINE static
        T* allocate_plenty (usize size) {
            return allocate(plenty_for_size(size));
        }

        [[gnu::nonnull(1)]] ALWAYS_INLINE static
        void deallocate (T* buf) {
            auto head = header(buf);
            lilac::deallocate(head,
                sizeof(SharableBufferHeader) + head->capacity * sizeof(T)
            );
        }
    };
}
}
