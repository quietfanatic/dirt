 // LILAC - a LIttle alLACator
 // Super simple small size singlethreaded slab allocator.
 //
 // Features:
 //  - Realtime-safe (O(1) worst case*)
 //  - Good best-case overhead (0.4%)
 //  - Small code size (less than 1k of optimized x64)
 //  - Some corruption detection when debug assertions are enabled
 //  - Basic stat collection with UNI_SLAB_PROFILE defined
 // Unconfirmed, but believed to be the case:
 //  - Very fast (much faster than malloc/free at least)
 //  - Low fragmentation for most use cases
 //  - Low cache pressure: usually only touches 3~4 data cache lines (including object
 //    being de/allocated) and 3~5 code cache lines.
 //  - Is written in C++ but could easily be ported to C
 // Caveats:
 //  - Absolutely no thread safety
 //  - Requires size for deallocation (can't replace malloc/free)
 //  - Only for allocations of 200 bytes or less (relays to malloc for larger)
 //  - Maximum total size of close to 16GB (64-bit) or 1GB (32-bit)
 //  - Only 8-byte alignment is guaranteed
 //  - Bad but bounded (and unlikely?) worst-case fragmentation
 //  - Very experimental and untested with anything but GCC on Linux x64
 //
 // Allocation never returns null or throws an exception.  The only error
 // condition is when the entire memory pool runs out, in which case the program
 // will be terminated.
 //
 // Currently, the size classes are of the form 24+16n.  This matches the
 // behavior of many 64-bit malloc implementations, though for different reasons
 // (malloc stores an 8-byte size and guarantees 16-byte alignment, whereas
 // uni::lilac picks this for synergy with SharableBuffer and ArrayInterface).
 //
 // You can achieve worst-case fragmentation by allocating a large amount of
 // same-sized objects, deallocating all but one per page, and then never
 // allocating more objects of that size again.  In this case, memory usage can
 // approach up to one page (4k by default) per object.
 //
 // There's also a slight performance hitch when allocating and deallocating a
 // single object over and over again if that allocation happens to reserve a
 // new page.  This always happens with the first object of any given size
 // class.  It'll probably still be faster than malloc/free though.
 //
 // (* The first allocation will reserve the entire pool's virtual address
 // space, and allocations that increase the total pool size may trigger
 // on-demand paging of physical memory, which may or may not be realtime-safe
 // depending on the kernel.)

#pragma once
#include <cstdlib>
#include "common.h"

namespace uni::lilac {

///// CUSTOMIZATION

 // Page size.  Making this higher improves performance and best-case overhead,
 // but makes worst-case fragmentation and potentially cache performance worse.
static constexpr usize page_size = 4096;

 // Maximum size of the pool in bytes.  I can't call std::aligned_alloc with
 // more than 16GB on my machine.  You probably want your program to crash
 // before it consumes 16GB anyway.
static constexpr usize pool_size =
    sizeof(void*) >= 8 ? 16ULL*1024*1024*1024 - page_size * 2
                       : 1*1024*1024*1024 - page_size * 2;

///// NOT CUSTOMIZATION

 // Changing these will require changing the code
static constexpr usize min_allocation_size = 24;
static constexpr usize max_allocation_size = 200;
static constexpr usize n_size_classes = 12;
static constexpr usize page_overhead = 16;
static constexpr usize page_usable_size = page_size - page_overhead;

void* allocate (usize);
void deallocate (void*, usize);
 // Dump some stats to stderr, but only if compiled with UNI_SLAB_PROFILE
void dump_profile ();

///// INTERNAL

namespace in {
    [[nodiscard, gnu::malloc, gnu::returns_nonnull]]
    void* internal_allocate (uint32 cat);
    [[gnu::nonnull(1)]]
    void internal_deallocate (void*, uint32 cat);
     // Valid sizes are:
     // 24, 40, 56, 72, 88, 104,
     // 120, 136, 152, 168, 184, 200
     // Some of these later sizes may not be very useful, but arranging them
     // linearly makes calculations much simpler.  It's also good to have 12 of
     // them, because this makes the global data structure fit neatly into one
     // cache line.
    constexpr uint32 size_class (uint32 size) {
        int32 scaled = int32(size - 9) >> 4;
        return scaled >= 0 ? scaled : 0;
    }
    constexpr uint32 class_size (uint32 cat) {
        return (cat << 4) + 24;
    }
} // in

 // Inline size calculation because the size is very likely to be statically
 // known.
[[nodiscard, gnu::malloc, gnu::returns_nonnull]] inline
void* allocate (usize size) {
    if (size <= max_allocation_size) {
        return in::internal_allocate(in::size_class(size));
    }
    else return std::malloc(size);
}
[[gnu::nonnull(1)]] inline
void deallocate (void* p, usize size) {
    if (size <= max_allocation_size) {
        in::internal_deallocate(p, in::size_class(size));
    }
    else std::free(p);
}

} // uni::lilac
