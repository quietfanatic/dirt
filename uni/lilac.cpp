#include "lilac.h"

#include "assertions.h"
#ifdef UNI_SLAB_PROFILE
#include <cstdio>
#endif

namespace uni::lilac {
namespace in {

struct alignas(page_size) Page {
     // Note: this must be at offset 0 for an optimization
    uint32 first_free_slot;
    uint32 bytes_used;  // Includes overhead
    uint32 next_page;
    uint32 prev_page;
    char data [page_usable_size];
};
static_assert(sizeof(Page) == page_size);

 // Try to fit this in one cache line
struct alignas(64) Global {
     // Base points BEFORE the allocated pool, so that base[0] is INVALID and
     // base[1] is the first page.
    Page* base = null;
    uint32 first_free_page = 0;
    uint32 first_untouched_page = -1;
    uint32 first_partial_pages [n_size_classes] = {};
};
static Global global;

#ifdef UNI_SLAB_PROFILE
struct Profile {
    usize pages_picked = 0;
    usize pages_emptied = 0;
    uint32 pages_current = 0;
    uint32 pages_most = 0;
    usize slots_allocated = 0;
    usize slots_deallocated = 0;
    uint32 slots_current = 0;
    uint32 slots_most = 0;
};
static Profile profiles [n_size_classes];
#endif

 // Noinline this and pass the argument along, so that internal_allocate can
 // tail call this.  This keeps internal_allocate from having to save cat on the
 // stack during the call to std::aligned_alloc.
[[gnu::cold]] NOINLINE static
void* init_pool (uint32 sc) {
     // Probably wasting nearly an entire page's worth of space for alignment.
     // Oh well.
    void* pool = std::aligned_alloc(page_size, pool_size);
    global.base = (Page*)pool - 1;
    global.first_untouched_page = 1;
     // This is what internal_allocate would do if we called back to it, and
     // it's actually a fairly small amount of compiled code, so just inline it
     // here, instead of jumping back to internal_allocate and polluting the
     // branch prediction tables.
    Page* page = global.base + global.first_untouched_page;
    global.first_partial_pages[sc] = global.first_untouched_page;
    global.first_untouched_page += 1;
    page->first_free_slot = 0;
    page->bytes_used = page_overhead + class_size(sc);
    page->next_page = 0;
    page->prev_page = 0;
     // Still need to twiddle the profile tables
#ifdef UNI_SLAB_PROFILE
    profiles[sc].pages_picked += 1;
    profiles[sc].pages_current += 1;
    if (profiles[sc].pages_most < profiles[sc].pages_current)
        profiles[sc].pages_most = profiles[sc].pages_current;
    profiles[sc].slots_allocated += 1;
    profiles[sc].slots_current += 1;
    if (profiles[sc].slots_most < profiles[sc].slots_current)
        profiles[sc].slots_most = profiles[sc].slots_current;
#endif
    return (char*)page + page_overhead;
}

[[noreturn]] static
void out_of_memory () { require(false); std::abort(); }

 // This complex of an expect() bogs down the optimizer
#ifndef NDEBUG
static
bool page_valid (Page* page, uint32 sc) {
    return (page->first_free_slot < page_size)
        & ((!page->first_free_slot)
         | ((page->first_free_slot % class_size(sc) == page_overhead)
          & (page->first_free_slot != *(uint32*)((char*)page + page->first_free_slot))
         )
        )
        & (page->bytes_used >= page_overhead)
        & (page->bytes_used <= page_size)
        & (page->bytes_used % class_size(sc) == page_overhead)
        & (page->next_page <= pool_size / page_size)
        & (page->next_page != (page - global.base))
        & (page->prev_page <= pool_size / page_size)
        & (page->prev_page != (page - global.base));
}
#else
ALWAYS_INLINE static
bool page_valid (Page*, uint32) { return true; }
#endif

void* internal_allocate (uint32 sc) {
    expect(sc < n_size_classes);
    Page* page;
    if (!global.first_partial_pages[sc]) [[unlikely]] {
         // We need a new page
        if (global.first_free_page) {
             // Use previously freed page
            page = global.base + global.first_free_page;
            global.first_partial_pages[sc] = global.first_free_page;
            global.first_free_page = page->next_page;
        }
        else {
             // > instead of >= because pages start at 1
            if (global.first_untouched_page > pool_size / page_size)
                [[unlikely]]
            {
                if (!global.base) {
                     // We haven't actually initialized yet
                    return init_pool(sc);
                }
                else [[unlikely]] out_of_memory();
            }
             // Get fresh page
            page = global.base + global.first_untouched_page;
            global.first_partial_pages[sc] = global.first_untouched_page;
            global.first_untouched_page += 1;
        }
         // Might as well shortcut the first allocation here, skipping the full
         // check later (we can't go from empty to full in one allocation).
        page->first_free_slot = 0;
        page->bytes_used = page_overhead + class_size(sc);
        page->next_page = 0;
        page->prev_page = 0;
#ifdef UNI_SLAB_PROFILE
        profiles[sc].pages_picked += 1;
        profiles[sc].pages_current += 1;
        if (profiles[sc].pages_most < profiles[sc].pages_current)
            profiles[sc].pages_most = profiles[sc].pages_current;
        profiles[sc].slots_allocated += 1;
        profiles[sc].slots_current += 1;
        if (profiles[sc].slots_most < profiles[sc].slots_current)
            profiles[sc].slots_most = profiles[sc].slots_current;
#endif
        return (char*)page + page_overhead;
    }
    else page = global.base + global.first_partial_pages[sc];
    expect(page_valid(page, sc));
     // Branched version
    //void* r;
    //if (page->first_free_slot) {
    //    r = (char*)page + page->first_free_slot;
    //    page->first_free_slot = *(uint32*)r;
    //}
    //else {
    //    r = (char*)page + page->bytes_used;
    //}
     // Branchless version
    uint32 slot = page->first_free_slot;
     // If first_free_slot is 0, this will write it to itself
    page->first_free_slot = *(uint32*)((char*)page + slot);
     // This has the effect of writing to slot if and only if it's zero
    slot |= page->bytes_used & (!!slot - 1);
    void* r = (char*)page + slot;
    page->bytes_used += class_size(sc);
#ifdef UNI_SLAB_PROFILE
    profiles[sc].slots_allocated += 1;
    profiles[sc].slots_current += 1;
    if (profiles[sc].slots_most < profiles[sc].slots_current)
        profiles[sc].slots_most = profiles[sc].slots_current;
#endif
    if (page->bytes_used > page_size - class_size(sc)) [[unlikely]] {
         // Page is full!  Take it out of the partial list.
         // With a large page size, this is likely to occupy 3 entries in the
         // same cache set.  Oh well.
        uint32 next = page->next_page;
        uint32 prev = page->prev_page;
        if (next) global.base[next].prev_page = prev;
        if (prev) global.base[prev].next_page = next;
        else global.first_partial_pages[sc] = next;
    }
    return r;
}

void internal_deallocate (void* p, uint32 sc) {
    expect(sc < n_size_classes);
     // Check that we own this pointer
     // This expect causes optimized build to load &global too early
#ifndef NDEBUG
    expect((char*)p > (char*)(global.base + 1)
        && (char*)p < (char*)(global.base + global.first_untouched_page)
    );
#endif
    Page* page = (Page*)((usize)p & ~(page_size - 1));
     // Check that pointer is aligned correctly
    expect(((char*)p - (char*)page) % class_size(sc) == page_overhead);
    expect(page_valid(page, sc));
     // Detect some cases of double-free
    expect(page->first_free_slot != (char*)p - (char*)page);
#ifndef NDEBUG
     // In debug mode, write trash to freed memory
    for (usize i = 0; i < class_size(sc) >> 3; i++) {
        ((uint64*)p)[i] = 0xbacafeeddeadbeef;
    }
#endif
     // Add to free list
    *(uint32*)p = page->first_free_slot;
    page->first_free_slot = (char*)p - (char*)page;
    page->bytes_used -= class_size(sc);
#ifdef UNI_SLAB_PROFILE
    profiles[sc].slots_deallocated += 1;
    profiles[sc].slots_current -= 1;
#endif
     // An integer math trick to do a range check with only one branch
    if (uint32(page->bytes_used - page_overhead - 1) >
        uint32(page_usable_size - class_size(sc) * 2 - 1)
    ) [[unlikely]] {
        if (page->bytes_used == page_overhead) {
             // Page is empty, take it out of the partial list
            uint32 next = page->next_page;
            uint32 prev = page->prev_page;
            if (next) global.base[next].prev_page = prev;
            if (prev) global.base[prev].next_page = next;
            else global.first_partial_pages[sc] = next;
             // And add it to the empty list.
            page->next_page = global.first_free_page;
            global.first_free_page = page - global.base;
#ifdef UNI_SLAB_PROFILE
            profiles[sc].pages_emptied += 1;
            profiles[sc].pages_current -= 1;
#endif
        }
        else {
             // Page went from full to partial, so put it on the partial list
            uint32 here = page - global.base;
            page->next_page = global.first_partial_pages[sc];
            page->prev_page = 0;
            if (page->next_page) {
                global.base[page->next_page].prev_page = here;
            }
            global.first_partial_pages[sc] = here;
        }
    }
}

static_assert(size_class(0) == 0);
static_assert(size_class(23) == 0);
static_assert(size_class(24) == 0);
static_assert(size_class(25) == 1);
static_assert(size_class(39) == 1);
static_assert(size_class(40) == 1);
static_assert(size_class(41) == 2);
static_assert(size_class(55) == 2);
static_assert(size_class(56) == 2);
static_assert(size_class(57) == 3);
static_assert(size_class(183) == 10);
static_assert(size_class(184) == 10);
static_assert(size_class(185) == 11);
static_assert(size_class(200) == 11);
static_assert(class_size(0) == 24);
static_assert(class_size(1) == 40);
static_assert(class_size(2) == 56);
static_assert(class_size(3) == 72);
static_assert(class_size(4) == 88);
static_assert(class_size(5) == 104);
static_assert(class_size(6) == 120);
static_assert(class_size(7) == 136);
static_assert(class_size(8) == 152);
static_assert(class_size(9) == 168);
static_assert(class_size(10) == 184);
static_assert(class_size(11) == 200);

} // in

void dump_profile () {
#ifdef UNI_SLAB_PROFILE
    std::fprintf(stderr, "\ncl siz page+ page- page= page> slot+ slot- slot= slot>\n");
    for (uint32 i = 0; i < n_size_classes; i++) {
        auto& p = in::profiles[i];
        std::fprintf(stderr, "%2u %3u %5zu %5zu %5u %5u %5zu %5zu %5u %5u\n",
            i, in::class_size(i),
            p.pages_picked, p.pages_emptied, p.pages_current, p.pages_most,
            p.slots_allocated, p.slots_deallocated, p.slots_current, p.slots_most
        );
    }
    std::fprintf(stderr, "most pool used: %zu\n",
        (in::global.first_untouched_page - 1) * page_size
    );
#endif
}

} // uni::lilac
