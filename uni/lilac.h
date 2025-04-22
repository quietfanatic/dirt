 // LILAC - a LIttle alLACator
 // Super simple small size singlethreaded slab allocator.
 //
 // Features:
 //  - Realtime-safe (O(1) worst case, depending on your OS's demand paging)
 //  - Good best-case overhead (0.4%)
 //  - Small code size (just over 1k compiled code and data)
 //  - Some corruption detection when debug assertions are enabled
 //  - Basic stat collection with UNI_LILAC_PROFILE defined
 // Unconfirmed, but believed to be the case:
 //  - Very fast (much faster than malloc/free at least)
 //  - Low fragmentation for most use cases
 //  - Low cache pressure: if the size is known at compile time, usually only
 //    touches 3~4 data cache lines (including object being de/allocated) and
 //    2~4 code cache lines (de/alloc pair).
 //  - Is written in C++ but could be ported to C straightforwardly, or even
 //    assembly.
 // Caveats:
 //  - Singlethreaded only (but possibly fast enough that you could put a mutex
 //    on the entire thing and still compete with an ordinary multithreaded
 //    allocator)
 //  - Only allocates up to 4080 bytes (relays larger requests to malloc)
 //  - Maximum total size of close to 8GB (64-bit) or 1GB (32-bit).  All the
 //    virtual address space is reserved at once, so it won't play well with
 //    other libraries that reserve huge address spaces on 32-bit systems.
 //  - Only 8-byte alignment is guaranteed
 //  - Bad but bounded (and unlikely?) worst-case external fragmentation
 //  - Worst-case internal fragmentation is 50% (1.5x).  Average for
 //    randomly-sized allocations is probably around 15%.
 //  - Cannot give memory back to the OS (but most allocators can't anyway)
 //  - Very experimental and untested with anything but GCC on Linux x64
 //  - Up to 145k space overhead due to paging granularity
 //
 // Allocation never returns null or throws an exception.  The only error
 // conditions are:
 //  - if the pool could not be reserved on the first allocation, or
 //  - when the entire memory pool runs out
 // in which cases the program will be terminated.  Requesting an allocation of
 // size 0 will return a non-null pointer to a minimally-allocated region.
 //
 // You can override the global operator new and operator delete by linking
 // lilac-global-override.cpp into the program.  Only do this is your entire
 // program is singlethreaded, or at least never allocates memory on another
 // thread.  You cannot replace malloc and free because lilac relies on malloc
 // to reserve its initial memory pool and to handle large allocations.
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

#pragma once
#include "assertions.h"

namespace uni::lilac {

///// API

 // Allocate some memory.
void* allocate (usize) noexcept;
 // Deallocate previously allocated memory.  The pointer must not be null.
void deallocate (void*, usize) noexcept;

 // Deallocate a block whose size you don't know.  May or may not be slower than
 // if the size is known.  The pointer may be null, in which case nothing
 // happens.
void deallocate_unknown_size (void*) noexcept;

 // This has the exact same behavior as allocate(), but is faster if the size is
 // known at compile-time (and probably slower if it isn't).
void* allocate_fixed_size (usize) noexcept;
 // Same as above but with deallocation.  You are allowed to mix-and-match
 // fixed_size and non-fixed_size allocation and deallocation functions.  The
 // pointer must not be null.
void deallocate_fixed_size (void*, usize) noexcept;

 // Equivalent of realloc.  Must not be passed a null pointer.  There's no
 // version where the old size is passed in (it's not that useful).  The current
 // implmentation never shrinks the memory region.
void* reallocate (void*, usize) noexcept;

 // Like allocate, but also returns the size of the allocated block.  The
 // returned size may be equal to the requested size, or it may be up to 50%
 // larger (1.5x).
struct Block {
    void* address;
    usize capacity;
};
Block allocate_block (usize) noexcept;

 // Dump some stats to stderr, but only if compiled with UNI_LILAC_PROFILE
void dump_profile () noexcept;

namespace in {

///// CUSTOMIZATION

 // Maximum size of the pool in bytes.  I can't call std::aligned_alloc with
 // more than 16GB on my machine.  You probably want your program to crash
 // before it consumes more than this anyway.
static constexpr usize pool_size =
    sizeof(void*) >= 8 ? 8ULL*1024*1024*1024 - 16384
                       : 1*1024*1024*1024 - 16384;

///// SIZE CLASSES

 // Page size.  Making this higher improves performance and best-case overhead,
 // but makes fragmentation worse.  You will need to adjust the size tables if
 // you change this.
static constexpr u32 page_size = 8192;
static constexpr u32 page_overhead = 32;
static constexpr u32 page_usable_size = page_size - page_overhead;

 // With the current implementation, 20 is a sweet spot for number of classes.
static constexpr u32 n_size_classes = 20;
static constexpr u32 largest_small = 12;
struct alignas(64) Tables {
     // These size classes are optimized for 8192 (8160 usable) byte pages, and
     // also for use with SharableBuffer and ArrayInterface, which like to have
     // sizes of 8+2^n.
    u16 class_sizes [n_size_classes] = {
     //  0   1   2   3   4   5   6
        16, 24, 32, 40, 56, 72, 88,
     //  7    8    9   10   11   12   13
        104, 136, 200, 272, 384, 544, 736,
     //  14    15    16    17    18    19
        1016, 1360, 1632, 2040, 2720, 4080
    };
    u8 small_classes_by_8 [69] = {
     //   0   8  16  24  32  40  48  56  64  72  80  88  96 104 112 120
         0,  0,  0,  1,  2,  3,  4,  4,  5,  5,  6,  6,  7,  7,  8,  8,
     // 128 136 144 152 160 168 176 184 192 200 208 216 224 232 240 248
         8,  8,  9,  9,  9,  9,  9,  9,  9,  9, 10, 10, 10, 10, 10, 10,
     // 256 264 272 280 288 296 304 312 320 328 336 344 352 360 368 376
        10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
     // 384 392 400 408 416 424 432 440 448 456 464 472 480 488 496 504
        11, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
     // 512 520 528 536 544
        12, 12, 12, 12, 12
    };
     // In theory we shouldn't need these 2 padding spaces, and can just
     // subtract 2 from the lookup index, but GCC-12 doesn't optimize the
     // subtraction out properly.  We've got room anyway.
    u8 medium_classes_by_div [15] = {
     //        2   3   4   5   6   7   8   9  10  11  12  13  14
        0, 0, 19, 18, 17, 16, 15, 15, 14, 14, 14, 13, 13, 13, 13
    };
};
constexpr Tables tables;
 // help gcc load table directly instead of referencing tables and adding an
 // offset
static constexpr const u16* class_sizes = tables.class_sizes;
static constexpr const u8* small_classes_by_8 = tables.small_classes_by_8;
static constexpr const u8* medium_classes_by_div = tables.medium_classes_by_div;

ALWAYS_INLINE constexpr
i32 get_size_class (usize size) {
    if (size <= class_sizes[largest_small]) [[likely]] {
        return small_classes_by_8[
            u32(size + 7) >> 3
        ];
    }
    else if (size <= class_sizes[n_size_classes-1]) {
        return medium_classes_by_div[
            page_usable_size / u32(size)
        ];
    }
    else [[unlikely]] return -1;
}

///// INTERNAL

struct Page;

[[nodiscard]]
Block allocate_small (Page*& first_partial, u32 slot_size) noexcept;
[[nodiscard]]
Block allocate_large (usize size) noexcept;
[[gnu::nonnull(1)]]
void deallocate_small (void*, Page*& first_partial, u32 slot_size) noexcept;
void deallocate_large (void*, usize size) noexcept;

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

///// INLINES (and optimization attributes)

[[
    nodiscard, gnu::malloc, gnu::returns_nonnull,
    gnu::alloc_size(1), gnu::assume_aligned(8)
]] ALWAYS_INLINE
void* allocate (usize size) noexcept {
    return allocate_block(size).address;
}

[[gnu::nonnull(1)]] ALWAYS_INLINE
void deallocate (void* p, usize) noexcept {
    expect(p);
     // Reading the size class from the page is faster than calculating it
    deallocate_unknown_size(p);
}

[[
    nodiscard, gnu::malloc, gnu::returns_nonnull,
    gnu::alloc_size(1), gnu::assume_aligned(8)
]] ALWAYS_INLINE
void* allocate_fixed_size (usize size) noexcept {
    i32 sc = in::get_size_class(size);
    if (sc >= 0) {
        auto& fp = in::global.first_partial_pages[sc];
        u32 slot_size = in::class_sizes[sc];
        return in::allocate_small(fp, slot_size).address;
    }
    else [[unlikely]] return in::allocate_large(size).address;
}

[[gnu::nonnull(1)]] ALWAYS_INLINE
void deallocate_fixed_size (void* p, usize size) noexcept {
    expect(p);
    i32 sc = in::get_size_class(size);
    if (sc >= 0) {
        auto& fp = in::global.first_partial_pages[sc];
        u32 slot_size = in::class_sizes[sc];
        in::deallocate_small(p, fp, slot_size);
    }
    else [[unlikely]] in::deallocate_large(p, size);
}

[[
    nodiscard, gnu::malloc, gnu::returns_nonnull,
    gnu::nonnull(1), gnu::alloc_size(2), gnu::assume_aligned(8)
]]
void* reallocate (void*, usize) noexcept;

[[nodiscard]]
Block allocate_block (usize size) noexcept;

} // uni::lilac
