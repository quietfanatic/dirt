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
 //    touches 3~4 data cache lines (including object being de/allocated) and 3~5
 //    code cache lines (de/alloc pair)
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
 // In this case, memory usage can approach up to one page (4k by default) per
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

 // Page size.  Making this higher improves performance and best-case overhead,
 // but makes fragmentation worse.
static constexpr uint32 page_size = 4096;

 // Maximum size of the pool in bytes.  I can't call std::aligned_alloc with
 // more than 16GB on my machine.  You probably want your program to crash
 // before it consumes 16GB anyway.
static constexpr usize pool_size =
    sizeof(void*) >= 8 ? 16ULL*1024*1024*1024 - page_size * 2
                       : 1*1024*1024*1024 - page_size * 2;

///// SIZE CLASSES

static constexpr uint32 n_size_classes = 16;
struct alignas(64) Tables {
     // These size classes are optimized for 4096 (4080 usable) byte pages, and
     // also for use with SharableBuffer and ArrayInterface, which like to have
     // sizes of 8+2^n.
    uint8 class_sizes_d8 [n_size_classes] = {
        16>>3, 24>>3, 32>>3, 40>>3, 56>>3, 72>>3, 104>>3, 136>>3,
        200>>3, 272>>3, 408>>3, 576>>3, 680>>3, 816>>3, 1016>>3, 1360>>3
    };
     // These tables will occupy three cache lines.  We could squeeze them into
     // two or one, by using a 4-bit encoding or chopping off the end, but it
     // would cost more code than it would save data.  Fixed-size allocations
     // will consult these tables at compile time.
    uint8 classes_by_8 [171] = {
     //   0   8  16  24  32  40  48  56  64  72  80  88  96 104 112 120
         0,  0,  0,  1,  2,  3,  4,  4,  5,  5,  6,  6,  6,  6,  7,  7,
     // 128 136 144 152 160 168 176 184 192 200 208 216 224 232 240 248
         7,  7,  8,  8,  8,  8,  8,  8,  8,  8,  9,  9,  9,  9,  9,  9,
     // 256 264 272 280 288 296 304 312 320 328 336 344 352 360 368 376
         9,  9,  9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
     // 384 392 400 408 416 424 432 440 448 456 464 472 480 488 496 504
        10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
     // 512 520 528 536 544 552 560 568 576 584 592 600 608 616 624 632
        11, 11, 11, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12,
     // 640 648 656 664 672 680 688 696 704 712 720 728 736 744 752 760
        12, 12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
     // 768 776 784 792 800 808 816 824 832 840 848 856 864 872 880 888
        13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 14, 14, 14,
     // 896 904 912 920 928 936 944 952 960 968 976 984 9921000 008 016
        14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
     //1024 032 040 048 056 064 072 080 088 096 104 112 120 128 136 144
        15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
     //1152 160 168 176 184 192 200 208 216 224 232 240 248 256 264 272
        15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
     //1280 288 296 304 312 320 328 336 344 352 360
        15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15
    };
};
constexpr Tables tables;

///// INTERNAL

[[
    nodiscard, gnu::malloc, gnu::returns_nonnull,
    gnu::alloc_size(2), gnu::assume_aligned(8)
]]
void* allocate_small (uint32& first_partial, uint32 slot_size);
[[
    nodiscard, gnu::malloc, gnu::returns_nonnull,
    gnu::alloc_size(1), gnu::assume_aligned(8)
]]
void* allocate_large (usize size);
[[gnu::nonnull(1)]]
void deallocate_small (void*, uint32& first_partial, uint32 slot_size);
[[gnu::nonnull(1)]]
void deallocate_large (void*, usize size);

 // Make the data structures visible so the lookup of first_partial_pages can be
 // done at compile-time.
struct Page;
struct alignas(64) Global {
    Page* base = null;
    uint32 first_free_page = 0;
    uint32 first_untouched_page = -1;
    uint32 first_partial_pages [n_size_classes] = {};
};
inline Global global;

} // in

///// INLINES

 // Encourage inlining small size calculation, because for small allocations the
 // size is likely to be known at compile-time, which lets the optimizer skip
 // the table lookups.
[[
    nodiscard, gnu::malloc, gnu::returns_nonnull,
    gnu::alloc_size(1), gnu::assume_aligned(8)
]] inline
void* allocate (usize size) {
    if (size <= 1360) {
        uint32 sc = in::tables.classes_by_8[uint32(size + 7) >> 3];
        uint32 slot_size = in::tables.class_sizes_d8[sc] << 3;
        return in::allocate_small(in::global.first_partial_pages[sc], slot_size);
    }
    else return in::allocate_large(size);
}
[[gnu::nonnull(1)]] inline
void deallocate (void* p, usize size) {
    if (size <= 1360) {
        uint32 sc = in::tables.classes_by_8[uint32(size + 7) >> 3];
        uint32 slot_size = in::tables.class_sizes_d8[sc] << 3;
        in::deallocate_small(p, in::global.first_partial_pages[sc], slot_size);
    }
    else in::deallocate_large(p, size);
}

} // uni::lilac
