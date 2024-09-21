 // LILAC - a LIttle alLACator
 // Super simple small size singlethreaded slab allocator.
 //
 // Features:
 //  - Realtime-safe (O(1) worst case*)
 //  - Good best-case overhead (0.4%, sizes not stored)
 //  - Small code size (less than 1k compiled code and data)
 //  - Some corruption detection when debug assertions are enabled
 //  - Basic stat collection with UNI_LILAC_PROFILE defined
 // Unconfirmed, but believed to be the case:
 //  - Very fast (much faster than malloc/free at least)
 //  - Low fragmentation for most use cases
 //  - Low cache pressure: if the size is known at compile time, usually only
 //    touches 3~4 data cache lines (including object being de/allocated) and
 //    3~4 code cache lines (de/alloc pair).
 //  - Is written in C++ but could be ported to C straightforwardly
 // Caveats:
 //  - Singlethreaded only
 //  - Requires size for deallocation (can't replace malloc/free)
 //  - Only allocates up to 1360 bytes (relays larger requests to malloc)
 //  - Maximum total size of close to 16GB (64-bit) or 1GB (32-bit).  All the
 //    virtual address space is reserved at once, so it won't play well with
 //    other libraries that reserve huge address spaces on 32-bit systems.
 //  - Only 8-byte alignment is guaranteed
 //  - Bad but bounded (and unlikely?) worst-case fragmentation
 //  - Cannot give memory back to the OS (but most allocators can't anyway)
 //  - Very experimental and untested with anything but GCC on Linux x64
 //
 // Allocation never returns null or throws an exception.  The only error
 // conditions are:
 //  - if the pool could not be reserved on the first allocation, or
 //  - when the entire memory pool runs out
 // in which cases the program will be terminated.
 //
 // Like other paging allocators, you can achieve worst-case fragmentation by
 // allocating a large amount of same-sized objects, deallocating all but one
 // per page, and then never allocating more objects of that size class again.
 // In this case, memory usage can approach up to one page (8k by default) per
 // object.
 //
 // There's also a slight performance hitch when allocating and deallocating a
 // single object over and over again if that allocation happens to be the first
 // or last slot in a page, including the first allocation of any given size
 // class.  It'll still beat the pants off any multithreaded allocator.
 //
 // (* The first allocation will reserve the entire pool's virtual address
 // space, and allocations that increase the total pool size may trigger
 // on-demand paging of physical memory, which may or may not be realtime-safe
 // depending on the kernel.)

#pragma once
#include "common.h"

namespace uni::lilac {

///// API

void* allocate (usize);
void deallocate (void*, usize);
 // Dump some stats to stderr, but only if compiled with UNI_LILAC_PROFILE
void dump_profile ();

namespace in {

///// CUSTOMIZATION

 // Maximum size of the pool in bytes.  I can't call std::aligned_alloc with
 // more than 16GB on my machine.  You probably want your program to crash
 // before it consumes 16GB anyway.
static constexpr usize pool_size =
    sizeof(void*) >= 8 ? 16ULL*1024*1024*1024 - 16384
                       : 1*1024*1024*1024 - 16384;

///// SIZE CLASSES

 // Page size.  Making this higher improves performance and best-case overhead,
 // but makes fragmentation worse.  You will need to adjust the size tables if
 // you change this.
static constexpr uint32 page_size = 8192;
static constexpr uint32 page_overhead = 32;
static constexpr uint32 page_usable_size = page_size - page_overhead;

static constexpr uint32 n_size_classes = 18;
static constexpr uint32 largest_small = 11;
struct alignas(64) Tables {
     // These size classes are optimized for 8192 (8160 usable) byte pages, and
     // also for use with SharableBuffer and ArrayInterface, which like to have
     // sizes of 8+2^n.
    uint16 class_sizes [n_size_classes] = {
     //  0   1   2   3   4   5   6
        16, 24, 32, 40, 56, 72, 104,
     //  7    8    9   10   11   12
        136, 200, 272, 408, 544, 680,
     // 13    14    15    16    17
        904, 1160, 1360, 1632, 2040
    };
    uint8 small_classes_by_8 [69] = {
     //   0   8  16  24  32  40  48  56  64  72  80  88  96 104 112 120
         0,  0,  0,  1,  2,  3,  4,  4,  5,  5,  6,  6,  6,  6,  7,  7,
     // 128 136 144 152 160 168 176 184 192 200 208 216 224 232 240 248
         7,  7,  8,  8,  8,  8,  8,  8,  8,  8,  9,  9,  9,  9,  9,  9,
     // 256 264 272 280 288 296 304 312 320 328 336 344 352 360 368 376
         9,  9,  9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
     // 384 392 400 408 416 424 432 440 448 456 464 472 480 488 496 504
        10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
     // 512 520 528 536 544
        11, 11, 11, 11, 11
    };
    uint8 medium_classes_by_div [12] = {
     //  4   5   6   7   8   9  10  11  12  13  14  15
        17, 16, 15, 14, 14, 13, 13, 13, 12, 12, 12, 12
    };
};
constexpr Tables tables;

ALWAYS_INLINE constexpr
int32 get_size_class (usize size) {
    if (size <= in::tables.class_sizes[in::largest_small]) {
        return in::tables.small_classes_by_8[
            uint32(size + 7) >> 3
        ];
    }
    else if (size <= in::tables.class_sizes[n_size_classes-1]) {
        return in::tables.medium_classes_by_div[
            page_usable_size / uint32(size) - 4
        ];
    }
    else return -1;
}

///// INTERNAL

struct Page;

[[
    nodiscard, gnu::malloc, gnu::returns_nonnull,
    gnu::alloc_size(2), gnu::assume_aligned(8)
]]
void* allocate_small (Page*& first_partial, uint32 slot_size);
[[
    nodiscard, gnu::malloc, gnu::returns_nonnull,
    gnu::alloc_size(1), gnu::assume_aligned(8)
]]
void* allocate_large (usize size);
[[gnu::nonnull(1)]]
void deallocate_small (void*, Page*& first_partial, uint32 slot_size);
[[gnu::nonnull(1)]]
void deallocate_large (void*, usize size);

 // Make the data structures visible so the lookup of first_partial_pages can be
 // done at compile-time.
struct alignas(64) Global {
    Page* first_partial_pages [n_size_classes] = {};
     // Put less-frequently-used things on the end
    Page* first_free_page = null;
    Page* first_untouched_page = null;
    Page* pool = null;
    Page* pool_end = null; // We have extra room, may as well cache this
};
inline Global global;

} // in

///// INLINES

 // Make these available at compile time (and not NOINLINE) so that the compiler
 // can do all the table lookups at compile time.  Ideally, the compiler will
 // not inline this if the size isn't known at compile time, and fortunately it
 // seems like it does if LTO is enabled.

[[
    nodiscard, gnu::malloc, gnu::returns_nonnull,
    gnu::alloc_size(1), gnu::assume_aligned(8)
]] inline
void* allocate (usize size) {
    int32 sc = in::get_size_class(size);
    if (sc >= 0) {
        uint32 slot_size = in::tables.class_sizes[sc];
        return in::allocate_small(in::global.first_partial_pages[sc], slot_size);
    }
    else return in::allocate_large(size);
}

[[gnu::nonnull(1)]] inline
void deallocate (void* p, usize size) {
    int32 sc = in::get_size_class(size);
    if (sc >= 0) {
        uint32 slot_size = in::tables.class_sizes[sc];
        in::deallocate_small(p, in::global.first_partial_pages[sc], slot_size);
    }
    else in::deallocate_large(p, size);
}

} // uni::lilac
