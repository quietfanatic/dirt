 // Super simple small-size singlethreaded slab allocator.  Very experimental
 // and untested.  Features:
 //  - Realtime allocation and deallocation (constant time worst case)
 //  - Pretty fast best-case performance (much faster than malloc/free anyway)
 //  - Very good best-case overhead (6.25%)
 //  - Low fragmentation for most use cases (unconfirmed)
 //  - Bad but bounded worst-case fragmentation
 //  - Absolutely no thread safety
 //  - Supports sizes of 8, 16, 24, 32, 40, 48, 56
 //  - Maximum total size of close to 16GB (64-bit) or 1GB (32-bit, untested)
 //  - Requires size for deallocation (can't implement free())
 //  - Returned pointers have 16-byte alignment if the object size is divisible
 //    by 16, otherwise they have 8-byte alignment
 //  - Some corruption detection when debug assertions are enabled
 //  - Basic stat collection with UNI_SLAB_PROFILE defined
 //  - Small code size (less than 500 bytes of optimized x64)
 //
 // You can achieve worst-case fragmentation by allocating a large amount of
 // same-sized objects, randomly deallocating 80~90% of them (depending on the
 // size), and then never allocating more objects of that size again.  In this
 // case, memory usage can approach 256 bytes per object.

#pragma once
#include <cstdlib>
#include "common.h"

namespace uni::slab {

static constexpr usize min_allocation_size = 8;
static constexpr usize max_allocation_size = 56;
static constexpr usize slab_size = 256;
static constexpr usize slab_overhead = 16;
static constexpr usize slab_usable_size = 240;
static constexpr usize n_size_categories = 7;
static constexpr usize allocation_limit =
    sizeof(void*) >= 8 ? 16ULL*1024*1024*1024 - slab_size * 2
                       : 1*1024*1024*1024 - slab_size * 2;
static constexpr usize max_slabs = allocation_limit / slab_size;
static_assert(allocation_limit % slab_size == 0);

void* allocate (usize);
void deallocate (void*, usize);

namespace in {
    [[nodiscard, gnu::malloc, gnu::returns_nonnull]]
    void* internal_allocate (uint32 cat);
    [[gnu::nonnull(1)]]
    void internal_deallocate (void*, uint32 cat);
     // Valid sizes are:
     //  8 -> 0 (30, 240)
     // 16 -> 1 (15, 240)
     // 24 -> 2 (10, 240)
     // 32 -> 3 (7, 224)
     // 40 -> 4 (6, 240)
     // 48 -> 5 (5, 240)
     // 56 -> 6 (4, 224)
     // Anything above this gets relegated to malloc
    constexpr uint32 size_category (uint32 size) {
        if (!size) [[unlikely]] return 0;
        return (size - 1) >> 3;
    }
    constexpr uint32 category_size (uint32 cat) {
        return (cat + 1) << 3;
    }
} // in

 // Inline size calculation because the size is very likely to be statically
 // known.
[[nodiscard, gnu::malloc, gnu::returns_nonnull]] inline
void* allocate (usize size) {
    if (size > max_allocation_size) return std::malloc(size);
    else return in::internal_allocate(in::size_category(size));
}
[[gnu::nonnull(1)]] inline
void deallocate (void* p, usize size) {
    if (size > max_allocation_size) std::free(p);
    else in::internal_deallocate(p, in::size_category(size));
}

 // Dump some stats to stderr, but only if UNI_SLAB_PROFILE is defined
#ifdef UNI_SLAB_PROFILE
void dump_profile ();
#else
inline void dump_profile () { }
#endif

} // uni::slab
