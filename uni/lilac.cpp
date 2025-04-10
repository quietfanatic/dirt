#include "lilac.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

//#define UNI_LILAC_PROFILE
#ifdef UNI_LILAC_PROFILE
#include <vector>
#endif

namespace uni::lilac {
namespace in {

struct alignas(64) Global {
    Page* first_partial_pages [n_size_classes] = {};
     // Put less-frequently-used things on the end
    Page* first_free_page = null;
    Page* first_untouched_page = null;
    Page* pool = null;
    Page* pool_end = null; // We have extra room, may as well cache this
};
Global global;

struct alignas(page_size) Page {
     // Note: this must be at offset 0 for a weird optimization
    u32 first_free_slot;
    u32 bytes_used;  // Includes overhead
    u32 size_class;
    u32 slot_size;
    Page* next_page;
    Page* prev_page;
};
static_assert(sizeof(Page) == page_size);

#ifdef UNI_LILAC_PROFILE
struct Profile {
    u64 pages_picked;
    u64 pages_emptied;
    u32 pages_current;
    u32 pages_most;
    u64 slots_allocated;
    u64 slots_deallocated;
    u32 slots_current;
    u32 slots_most;
};
static Profile profiles [n_size_classes] = {};
static Profile total = {};
static usize slot_bytes_current = 0;
static usize slot_bytes_most = 0;
static u64 oversize_allocated = 0;
static u64 oversize_deallocated = 0;
static u64 oversize_current = 0;
static u64 oversize_most = 0;
#endif

[[noreturn]] ALWAYS_INLINE static
void out_of_memory () {
    std::fprintf(stderr, "uni::lilac panic: out of memory\n");
    std::abort();
}

[[noreturn]] ALWAYS_INLINE static
void malloc_failed () {
    std::fprintf(stderr, "uni::lilac panic: malloc failed\n");
    std::abort();
}

NOINLINE static
Block init_page (Page*& first_partial, u32 slot_size, Page* page) noexcept {
     // Might as well shortcut the first allocation here, skipping the full
     // check later (we can't go from empty to full in one allocation).  This
     // also avoids writing a non-zero constant to these fields, which would
     // cause the compiler to load a 16-byte vector constant from static data.
    first_partial = page;
    page->first_free_slot = 0;
    page->bytes_used = page_overhead + slot_size;
    page->size_class = &first_partial - global.first_partial_pages;
    page->slot_size = slot_size;
    page->next_page = null;
    page->prev_page = null;
     // Still need to twiddle the profile tables
#ifdef UNI_LILAC_PROFILE
    u32 sc = &first_partial - global.first_partial_pages;
    profiles[sc].pages_picked += 1;
    total.pages_picked += 1;
    profiles[sc].pages_current += 1;
    total.pages_current += 1;
    if (profiles[sc].pages_most < profiles[sc].pages_current)
        profiles[sc].pages_most = profiles[sc].pages_current;
    if (total.pages_most < total.pages_current)
        total.pages_most = total.pages_current;
    profiles[sc].slots_allocated += 1;
    total.slots_allocated += 1;
    profiles[sc].slots_current += 1;
    total.slots_current += 1;
    if (profiles[sc].slots_most < profiles[sc].slots_current)
        profiles[sc].slots_most = profiles[sc].slots_current;
    if (total.slots_most < total.slots_current)
        total.slots_most = total.slots_current;
    slot_bytes_current += slot_size;
    if (slot_bytes_most < slot_bytes_current)
        slot_bytes_most = slot_bytes_current;
#endif
    return Block{(char*)page + page_overhead, slot_size};
}

 // Noinline this and pass the argument along, so that allocate_small can tail
 // call this.  This keeps allocate_small from having to save stuff on the stack
 // during the call to std::aligned_alloc.
 // I don't know whether it's appropriate to put [[gnu::cold]] on a function
 // that's always called exactly once.
NOINLINE static
Block init_pool (Page*& first_partial, u32 slot_size) noexcept {
     // Probably wasting nearly an entire page's worth of space for alignment.
     // Oh well.
    global.pool = (Page*)std::aligned_alloc(page_size, pool_size);
    if (!global.pool) malloc_failed();
    global.pool_end = global.pool + pool_size / page_size;
    global.first_untouched_page = global.pool;
    Page* page = global.first_untouched_page;
    global.first_untouched_page += 1;
    return init_page(first_partial, slot_size, page);
}

 // This complex of an expect() bogs down the optimizer
#ifndef NDEBUG
static
bool page_valid (Page* page, u32 slot_size) {
    return (page->first_free_slot < page_size)
        & ((!page->first_free_slot)
         | (((page->first_free_slot - page_overhead) % slot_size == 0)
          & (page->first_free_slot != *(u32*)((char*)page + page->first_free_slot))
         )
        )
        & (page->bytes_used >= page_overhead)
        & (page->bytes_used <= page_size)
        & ((page->bytes_used - page_overhead) % slot_size == 0)
        & (page->slot_size == slot_size)
        & (page->next_page != page)
        & (page->prev_page != page);
}
#else
ALWAYS_INLINE static
bool page_valid (Page*, u32) { return true; }
#endif

static
Block allocate_page (Page*& first_partial, u32 slot_size) noexcept {
    if (global.first_free_page) {
         // Use previously freed page
        Page* page = global.first_free_page;
        global.first_free_page = page->next_page;
        return init_page(first_partial, slot_size, page);
    }
    else {
        if (global.first_untouched_page >= global.pool_end) [[unlikely]] {
            if (!global.first_untouched_page) {
                 // We haven't actually initialized yet
                return init_pool(first_partial, slot_size);
            }
            else [[unlikely]] out_of_memory();
        }
         // Get fresh page
        Page* page = global.first_untouched_page;
        global.first_untouched_page += 1;
        return init_page(first_partial, slot_size, page);
    }
}

NOINLINE
Block allocate_small (u32 size_class) noexcept {
    Page*& first_partial = in::global.first_partial_pages[size_class];
    u32 slot_size = in::class_sizes[size_class];
    if (!first_partial) [[unlikely]] {
        return allocate_page(first_partial, slot_size);
    }
    Page* page = first_partial;
    expect(page_valid(page, slot_size));
#ifdef UNI_LILAC_PROFILE
    u32 sc = &first_partial - global.first_partial_pages;
    profiles[sc].slots_allocated += 1;
    total.slots_allocated += 1;
    profiles[sc].slots_current += 1;
    total.slots_current += 1;
    if (profiles[sc].slots_most < profiles[sc].slots_current)
        profiles[sc].slots_most = profiles[sc].slots_current;
    if (total.slots_most < total.slots_current)
        total.slots_most = total.slots_current;
    slot_bytes_current += slot_size;
    if (slot_bytes_most < slot_bytes_current)
        slot_bytes_most = slot_bytes_current;
#endif

     // Branched version.  Ideally this compiles to a single forward branch,
     // with small resulting code, but it's not clear how predictable this is.
     // It seems to go through periods of relative predictability and
     // unpredictability.  A two-bit counter predictor guesses this correctly
     // ~90% of the time.  A more complex predictor may or may not do better
     // than that.
//    void* r = (char*)page + page->bytes_used;
//    page->bytes_used += slot_size;
//    if (page->first_free_slot) {
//        r = (char*)page + page->first_free_slot;
//        page->first_free_slot = *(u32*)r;
//    }
     // Branchless version.  That said, after much perturbation I finally got
     // the cmov to optimize not-so-terribly-badly.  Although it's still not
     // quite optimal (GCC does the cmov with 64-bit size unnecessarily), this
     // now barely beats the branched version in code size (insts and bytes).
     //
     // Do this add before the cmov to avoid an extra register copy.
    u32 new_bu = page->bytes_used + slot_size;
    u32 slot = page->first_free_slot;
    if (!slot) slot = page->bytes_used;
     // Doing this before the load below prevents the compiler from using vector
     // instructions to merge the stores together (which is 1 more instruction
     // and 10 more code bytes just to save one store).
    page->bytes_used = new_bu;
     // If first_free_slot is 0, this will write it to itself
    page->first_free_slot = *(u32*)((char*)page + page->first_free_slot);
     // Do potentially-cache-missing memory accesses before this branch
    if (new_bu + slot_size > page_size) [[unlikely]] {
         // Page is full!  Take it out of the partial list.
         // With a large page size, this is likely to occupy 3 entries in the
         // same cache set.  Oh well.
        Page* next = page->next_page;
        Page* prev = page->prev_page;
        Page* prev_target = next ? next : page;
        prev_target->prev_page = prev;
        Page** next_target = prev ? &prev->next_page : &first_partial;
        *next_target = next;
    }
    return Block{(char*)page + slot, slot_size};
}

Block allocate_large (usize size) noexcept {
#ifdef UNI_LILAC_PROFILE
    oversize_allocated += 1;
    oversize_current += 1;
    if (oversize_most < oversize_current)
        oversize_most = oversize_current;
#endif
    void* r = std::malloc(size);
     // Usually I prefer to just let it segfault when malloc returns null, but
     // we've established a contract that our API never returns null, and I
     // don't really want to break it.
    if (!r) malloc_failed();
     // Rounding up to the allocation granularity isn't worth it.  For large
     // allocations you won't miss 7 extra bytes that you didn't even request.
    return Block{r, size};
}

} using namespace in;

NOINLINE
Block allocate_block (usize size) noexcept {
    i32 sc = get_size_class(size);
    if (sc >= 0) {
        return allocate_small(sc);
    }
    else [[unlikely]] return allocate_large(size);
}
namespace in {

NOINLINE
void deallocate_small (void* p, u32 size_class) noexcept {
    u32 slot_size = class_sizes[size_class];
     // Check that we own this pointer
     // This expect causes optimized build to load &global too early
#ifndef NDEBUG
    expect((char*)p > (char*)global.pool
        && (char*)p < (char*)global.first_untouched_page
    );
#endif
    Page* page = (Page*)((usize)p & ~usize(page_size - 1));
     // Check that pointer is aligned correctly
    expect(((char*)p - (char*)page - page_overhead) % slot_size == 0);
    expect(page_valid(page, slot_size));
#ifndef NDEBUG
     // In debug mode, write trash to freed memory
    for (u32 i = 0; i < slot_size >> 3; i++) {
        ((u64*)p)[i] = 0xbacafeeddeadbeef;
    }
#endif
     // Add to free list
    *(u32*)p = page->first_free_slot;
    page->first_free_slot = (char*)p - (char*)page;
    page->bytes_used -= slot_size;
#ifdef UNI_LILAC_PROFILE
    u32 sc = &first_partial - global.first_partial_pages;
    profiles[sc].slots_deallocated += 1;
    total.slots_deallocated += 1;
    profiles[sc].slots_current -= 1;
    total.slots_current -= 1;
    slot_bytes_current -= slot_size;
#endif
     // It's possible to do a math trick to merge these branches into one, but
     // it ends up using more instructions even in the likely case.
    if (page->bytes_used == page_overhead) [[unlikely]] {
        Page*& first_partial = global.first_partial_pages[size_class];
         // Page is empty, take it out of the partial list
        Page* next = page->next_page;
        Page* prev = page->prev_page;
        Page* prev_target = next ? next : page;
        prev_target->prev_page = prev;
        Page** next_target = prev ? &prev->next_page : &first_partial;
        *next_target = next;
         // And add it to the empty list.
        page->next_page = global.first_free_page;
        global.first_free_page = page;
#ifdef UNI_LILAC_PROFILE
        u32 sc = &first_partial - global.first_partial_pages;
        profiles[sc].pages_emptied += 1;
        total.pages_emptied += 1;
        profiles[sc].pages_current -= 1;
        total.pages_current -= 1;
#endif
    }
    else if (page->bytes_used + slot_size * 2 > page_size) [[unlikely]] {
        Page*& first_partial = global.first_partial_pages[size_class];
         // Page went from full to partial, so put it on the partial list
        Page* first = first_partial;
        Page* target = first ? first : page;
        target->prev_page = page;
        page->next_page = first;
        page->prev_page = null;
        first_partial = page;
    }
}

void deallocate_large (void* p, usize) noexcept {
#ifdef UNI_LILAC_PROFILE
    if (p) {
        oversize_deallocated += 1;
        oversize_current -= 1;
    }
#endif
    std::free(p);
}

} using namespace in;

NOINLINE
void deallocate_unknown_size (void* p) noexcept {
     // p is allowed to be null, in which case it will be passed to free, which
     // allows null pointers.
    if ((char*)p >= (char*)global.pool
     && (char*)p < (char*)global.pool_end
    ) {
        Page* page = (Page*)((usize)p & ~usize(page_size - 1));
        deallocate_small(p, page->size_class);
    }
    else [[unlikely]] deallocate_large(p, 0);
}

namespace in {
 // noinline-and-tail-call, so that reallocate doesn't have to make a stack
 // frame if it doesn't have to.
NOINLINE
void* reallocate_from_small (void* p, usize s, u32 old_s) {
    void* r = allocate(s);
    std::memcpy(r, p, old_s);
    deallocate_unknown_size(p);
    return r;
}
} // in

NOINLINE
void* reallocate (void* p, usize s) noexcept {
    if ((char*)p >= (char*)global.pool
     && (char*)p < (char*)global.pool_end
    ) [[likely]] {
        Page* page = (Page*)((usize)p & ~usize(page_size - 1));
        u32 old_s = page->slot_size;
        if (s > old_s) {
            return reallocate_from_small(p, s, old_s);
        }
        else return p;
    }
    else {
        if (s > class_sizes[n_size_classes-1]) {
            return std::realloc(p, s);
        }
        else return p;
    }
}

void dump_profile () noexcept {
#ifdef UNI_LILAC_PROFILE
    std::fprintf(stderr,
        "\ncl size page+ page- page= page> slot+ slot- slot= slot>  bytes+  bytes- bytes= bytes>\n");
    u64 tb_a = 0; u64 tb_d = 0;
    u64 tb_c = 0; u64 tb_m = 0;
    for (u32 i = 0; i < n_size_classes; i++) {
        auto& p = profiles[i];
        u64 s = class_sizes[i];
        std::fprintf(stderr, "%2u %4u %5zu %5zu %5u %5u %5zu %5zu %5u %5u %7zu %7zu %6zu %6zu\n",
            i, class_sizes[i],
            p.pages_picked, p.pages_emptied,
            p.pages_current, p.pages_most,
            p.slots_allocated, p.slots_deallocated,
            p.slots_current, p.slots_most,
            p.slots_allocated * s, p.slots_deallocated * s,
            p.slots_current * s, p.slots_most * s
        );
        tb_a += p.slots_allocated * s; tb_d += p.slots_deallocated * s;
        tb_c += p.slots_current * s; tb_m += p.slots_most * s;
    }
    std::fprintf(stderr, "  total %5zu %5zu %5u %5u %5zu %5zu %5u %5u %7zu %7zu %6zu %6zu\n",
        total.pages_picked, total.pages_emptied,
        total.pages_current, total.pages_most,
        total.slots_allocated, total.slots_deallocated,
        total.slots_current, total.slots_most,
        tb_a, tb_d, tb_c, tb_m
    );
    std::fprintf(stderr, "over+ %zu over- %zu over= %zu over> %zu\n",
        oversize_allocated, oversize_deallocated,
        oversize_current, oversize_most
    );
    std::fprintf(stderr, "most slot bytes %zu\nmost page bytes %zu\n",
        slot_bytes_most,
        (global.first_untouched_page - global.pool) * page_size
    );
#endif
}

} // uni::lilac
