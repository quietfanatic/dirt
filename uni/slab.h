 // Super simple small-size singlethreaded slab allocator.
 //
 // Features:
 //  - Realtime-safe (O(1) worst case*)
 //  - Good best-case overhead (6.25%, no size stored)
 //  - Small code size (500 bytes of optimized x64)
 //  - Some corruption detection when debug assertions are enabled
 //  - Basic stat collection with UNI_SLAB_PROFILE defined
 // Unconfirmed, but believed to be the case:
 //  - Very fast (much faster than malloc/free at least)
 //  - Low fragmentation for most use cases
 // Caveats:
 //  - Only for allocations of 56 bytes or less
 //  - Absolutely no thread safety
 //  - Bad but bounded worst-case fragmentation
 //  - Maximum total size of close to 16GB (64-bit) or 1GB (32-bit, untested)
 //  - Requires size for deallocation (can't replace free())
 //  - Very experimental and untested on anything but GCC on Linux
 //
 // Returned pointers have 16-byte alignment if the allocation size is divisible
 // by 16, otherwise they have 8-byte alignment.
 //
 // You can achieve worst-case fragmentation by allocating a large amount of
 // same-sized objects, randomly deallocating 80~90% of them (depending on the
 // size), and then never allocating more objects of that size again.  In this
 // case, memory usage can approach 256 bytes per object.
 //
 // Allocation never returns null or throws an exception.  The only error
 // condition is when the entire memory pool runs out, in which case the program
 // will be terminated.
 //
 // (* The first allocation will reserve the entire pool's virtual address
 // space, and allocations that increase the total pool size may trigger
 // on-demand paging of physical memory, which may or may not be realtime-safe
 // depending on the kernel.)

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
