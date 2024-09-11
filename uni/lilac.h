 // LILAC - a LIttle alLACator
 // Super simple small size singlethreaded slab allocator.
 //
 // Features:
 //  - Realtime-safe (O(1) worst case*)
 //  - Good best-case overhead (0.4%)
 //  - Small code size (less than 1k of optimized x64)
 //  - Some corruption detection when debug assertions are enabled
 //  - Basic stat collection with UNI_LILAC_PROFILE defined
 // Unconfirmed, but believed to be the case:
 //  - Very fast (much faster than malloc/free at least)
 //  - Low fragmentation for most use cases
 //  - Low cache pressure: usually only touches 3~5 data cache lines (including
 //    object being de/allocated) and 3~5 code cache lines.
 //  - Is written in C++ but could easily be ported to C
 // Caveats:
 //  - Absolutely no thread safety
 //  - Requires size for deallocation (can't replace malloc/free)
 //  - Only for allocations of 408 bytes or less (relays to malloc for larger)
 //  - Maximum total size of close to 16GB (64-bit) or 1GB (32-bit)
 //  - Only 8-byte alignment is guaranteed
 //  - Bad but bounded (and unlikely?) worst-case fragmentation
 //  - Very experimental and untested with anything but GCC on Linux x64
 //
 // Allocation never returns null or throws an exception.  The only error
 // condition is when the entire memory pool runs out, in which case the program
 // will be terminated.
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

///// API

void* allocate (usize);
void deallocate (void*, usize);
 // Dump some stats to stderr, but only if compiled with UNI_LILAC_PROFILE
void dump_profile ();

///// INTERNAL

namespace in {

static constexpr usize min_allocation_size = 8;
static constexpr usize max_allocation_size = 408;
static constexpr usize n_size_classes = 12;
 // These size classes are optimized for 4096 (4080) byte pages.
 // Having 12 size classes allows the global data structure to fit into a single
 // 64 byte cache line, and going up to 408 makes these tables fit into one line
 // as well.
struct alignas(64) Tables {
    uint8 class_to_words [12];
    uint8 class_from_words [52];
};
constexpr Tables tables = {
    {
        8>>3, 16>>3, 24>>3, 32>>3, 40>>3, 56>>3, 72>>3,
        104>>3, 136>>3, 200>>3, 272>>3, 408>>3,
    }, {
     //   0   8  16  24  32  40  48  56  64  72  80  88  96 104 112 120
         0,  0,  1,  2,  3,  4,  5,  5,  6,  6,  7,  7,  7,  7,  8,  8,
     // 128 136 144 152 160 168 176 184 192 200 208 216 224 232 240 248
         8,  8,  9,  9,  9,  9,  9,  9,  9,  9, 10, 10, 10, 10, 10, 10,
     // 256 264 272 280 288 296 304 312 320 328 336 344 352 360 368 376
        10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
     // 384 392 400 408
        11, 11, 11, 11
    }
};
static constexpr usize page_overhead = 16;
static constexpr usize page_usable_size = page_size - page_overhead;

[[nodiscard, gnu::malloc, gnu::returns_nonnull]]
void* internal_allocate (uint32 sc, uint32 slot_size);
[[gnu::nonnull(1)]]
void internal_deallocate (void*, uint32 sc, uint32 slot_size);

} // in

 // Inline size calculation because the size is likely to be statically known.
[[nodiscard, gnu::malloc, gnu::returns_nonnull]] inline
void* allocate (usize size) {
    if (size <= in::max_allocation_size) {
        uint32 sc = in::tables.class_from_words[(size + 7) >> 3];
        uint32 slot_size = in::tables.class_to_words[sc] << 3;
        return in::internal_allocate(sc, slot_size);
    }
    else return std::malloc(size);
}
[[gnu::nonnull(1)]] inline
void deallocate (void* p, usize size) {
    if (size <= in::max_allocation_size) {
        uint32 sc = in::tables.class_from_words[(size + 7) >> 3];
        uint32 slot_size = in::tables.class_to_words[sc] << 3;
        in::internal_deallocate(p, sc, slot_size);
    }
    else std::free(p);
}

} // uni::lilac
