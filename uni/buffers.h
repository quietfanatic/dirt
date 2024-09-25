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

        static constexpr usize max_capacity = 0x80000000;

         // Round up a requested size a little to avoid excessive reallocations
        static constexpr usize capacity_for_size (usize size) {
            require(size <= max_capacity);
             // Give up on rounding up non-power-of-two sizes.
            usize mask = sizeof(T) == 1 ? 7
                       : sizeof(T) == 2 ? 3
                       : sizeof(T) == 4 ? 1
                       : 0;
            usize cap = (size + mask) & ~mask;
            return cap;
        }

        static constexpr usize min_capacity = capacity_for_size(1);

         // Round up the requested size to a power of two, anticipating
         // continual growth.
        static constexpr usize plenty_for_size (usize size) {
            require(size <= max_capacity);
            constexpr usize min = min_capacity < 4 ? 4 : min_capacity;
            if (size <= min) return min;
            else {
                 // This should be fast on any modern processor
                return std::bit_ceil(uint32(size));
            }
        }

         // Allocate rounding up a little
        [[gnu::malloc, gnu::returns_nonnull]] ALWAYS_INLINE static
        T* allocate (usize size) {
            return allocate_exact(capacity_for_size(size));
        }

         // Allocate rounding up to a power of two
        [[gnu::malloc, gnu::returns_nonnull]] ALWAYS_INLINE static
        T* allocate_plenty (usize size) {
            return allocate_exact(plenty_for_size(size));
        }

         // Allocate the exact amount without rounding (may waste space)
        [[gnu::malloc, gnu::returns_nonnull]] static
        T* allocate_exact (usize cap) {
            static_assert(
                alignof(T) <= 8,
                "SharableBuffer and uni array types with elements that have "
                "align > 8 are NYI."
            );
             // Use uint64 instead of usize because on 32-bit platforms we need
             // to make sure we don't overflow usize.
            uint64 bytes = sizeof(SharableBufferHeader) + (uint64)cap * sizeof(T);
            require(bytes <= usize(-1));
            auto header = (SharableBufferHeader*)lilac::allocate(bytes);
            const_cast<uint32&>(header->capacity) = cap;
            header->ref_count = 1;
            return (T*)(header + 1);
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
