#include "lilac.h"

#include <cstdlib>
#include "assertions.h"

//#define UNI_LILAC_PROFILE
#ifdef UNI_LILAC_PROFILE
#include <cstdio>
#include <vector>
#endif

namespace uni::lilac {
namespace in {

static constexpr uint32 page_overhead = 16;
static constexpr uint32 page_usable_size = page_size - page_overhead;

struct alignas(page_size) Page {
     // Note: this must be at offset 0 for a weird optimization
    uint32 first_free_slot;
    uint32 bytes_used;  // Includes overhead
     // 1-based index instead of a raw pointer.  Using raw pointers is slightly
     // fewer instructions, but uses more overhead and global data.
    uint32 next_page;
    uint32 prev_page;
    char data [page_usable_size];
};
static_assert(sizeof(Page) == page_size);

#ifdef UNI_LILAC_PROFILE
struct Profile {
    uint64 pages_picked = 0;
    uint64 pages_emptied = 0;
    uint32 pages_current = 0;
    uint32 pages_most = 0;
    uint64 slots_allocated = 0;
    uint64 slots_deallocated = 0;
    uint32 slots_current = 0;
    uint32 slots_most = 0;
};
static Profile profiles [n_size_classes];
static uint64 oversize_allocated = 0;
static uint64 oversize_deallocated = 0;
static uint64 oversize_current = 0;
static uint64 oversize_most = 0;
//static std::vector<bool> decisions;
#endif

 // Noinline this and pass the argument along, so that allocate_small can tail
 // call this.  This keeps allocate_small from having to save cat on the stack
 // during the call to std::aligned_alloc.
 // I don't know whether it's appropriate to put [[gnu::cold]] on a function
 // that's always called exactly once.
NOINLINE static
void* init_pool (uint32& first_partial, uint32 slot_size) {
     // Probably wasting nearly an entire page's worth of space for alignment.
     // Oh well.
    void* pool = std::aligned_alloc(page_size, pool_size);
    global.base = (Page*)pool - 1;
    global.first_untouched_page = 1;
     // This is what allocate_small would do if we called back to it, and it's
     // actually a fairly small amount of compiled code, so just inline it here,
     // instead of jumping back to allocate_small and polluting the branch
     // prediction tables.
    Page* page = global.base + global.first_untouched_page;
    first_partial = global.first_untouched_page;
    global.first_untouched_page += 1;
    page->first_free_slot = 0;
    page->bytes_used = page_overhead + slot_size;
    page->next_page = 0;
    page->prev_page = 0;
     // Still need to twiddle the profile tables
#ifdef UNI_LILAC_PROFILE
    uint32 sc = &first_partial - global.first_partial_pages;
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
bool page_valid (Page* page, uint32 slot_size) {
    return (page->first_free_slot < page_size)
        & ((!page->first_free_slot)
         | (((page->first_free_slot - page_overhead) % slot_size == 0)
          & (page->first_free_slot != *(uint32*)((char*)page + page->first_free_slot))
         )
        )
        & (page->bytes_used >= page_overhead)
        & (page->bytes_used <= page_size)
        & ((page->bytes_used - page_overhead) % slot_size == 0)
        & (page->next_page <= pool_size / page_size)
        & (page->next_page != (page - global.base))
        & (page->prev_page <= pool_size / page_size)
        & (page->prev_page != (page - global.base));
}
#else
ALWAYS_INLINE static
bool page_valid (Page*, uint32) { return true; }
#endif


NOINLINE
void* allocate_small (uint32& first_partial, uint32 slot_size) {
    if (!first_partial) [[unlikely]] {
         // We need a new page
        Page* page;
        if (global.first_free_page) {
             // Use previously freed page
            page = global.base + global.first_free_page;
            first_partial = global.first_free_page;
            global.first_free_page = page->next_page;
        }
        else {
             // > instead of >= because pages start at 1
            if (global.first_untouched_page > pool_size / page_size)
                [[unlikely]]
            {
                if (!global.base) {
                     // We haven't actually initialized yet
                    return init_pool(first_partial, slot_size);
                }
                else [[unlikely]] out_of_memory();
            }
             // Get fresh page
            page = global.base + global.first_untouched_page;
            first_partial = global.first_untouched_page;
            global.first_untouched_page += 1;
        }
         // Might as well shortcut the first allocation here, skipping the full
         // check later (we can't go from empty to full in one allocation).
         // This also avoids writing a non-zero constant to these fields, which
         // would cause the compiler to load a 16-byte vector constant from
         // static data.
        page->first_free_slot = 0;
        page->bytes_used = page_overhead + slot_size;
        page->next_page = 0;
        page->prev_page = 0;
#ifdef UNI_LILAC_PROFILE
        uint32 sc = &first_partial - global.first_partial_pages;
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

    Page* page = global.base + first_partial;
    expect(page_valid(page, slot_size));
#ifdef UNI_LILAC_PROFILE
    uint32 sc = &first_partial - global.first_partial_pages;
    profiles[sc].slots_allocated += 1;
    profiles[sc].slots_current += 1;
    if (profiles[sc].slots_most < profiles[sc].slots_current)
        profiles[sc].slots_most = profiles[sc].slots_current;
//    decisions.push_back(page->first_free_slot);
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
//        page->first_free_slot = *(uint32*)r;
//    }
     // Branchless version.  That said, after much perturbation I finally got
     // the cmov to optimize not-so-terribly-badly.  Although it's still not
     // quite optimal (GCC does the cmov with 64-bit size unnecessarily), this
     // now barely beats the branched version in code size (insts and bytes).
     //
     // Do this add before the cmov to avoid an extra register copy.
    uint32 new_bu = page->bytes_used + slot_size;
    uint32 slot = page->first_free_slot;
    if (!slot) slot = page->bytes_used;
     // Doing this before the load below prevents the compiler from using vector
     // instructions to merge the stores together (which is 1 more instruction
     // and 10 more code bytes just to save one store).
    page->bytes_used = new_bu;
     // If first_free_slot is 0, this will write it to itself
    page->first_free_slot = *(uint32*)((char*)page + page->first_free_slot);
     // Do potentially-cache-missing memory accesses before this branch
    if (new_bu + slot_size > page_size) [[unlikely]] {
         // Page is full!  Take it out of the partial list.
         // With a large page size, this is likely to occupy 3 entries in the
         // same cache set.  Oh well.
        uint32 next = page->next_page;
        uint32 prev = page->prev_page;
        if (next) global.base[next].prev_page = prev;
        if (prev) global.base[prev].next_page = next;
        else first_partial = next;
    }
    return (char*)page + slot;
}

void* allocate_large (usize size) {
#ifdef UNI_LILAC_PROFILE
    oversize_allocated += 1;
    oversize_current += 1;
    if (oversize_most < oversize_current)
        oversize_most = oversize_current;
#endif
    return std::malloc(size);
}

NOINLINE
void deallocate_small (void* p, uint32& first_partial, uint32 slot_size) {
     // Check that we own this pointer
     // This expect causes optimized build to load &global too early
#ifndef NDEBUG
    expect((char*)p > (char*)(global.base + 1)
        && (char*)p < (char*)(global.base + global.first_untouched_page)
    );
#endif
    Page* page = (Page*)((usize)p & ~usize(page_size - 1));
     // Check that pointer is aligned correctly
    expect(((char*)p - (char*)page - page_overhead) % slot_size == 0);
    expect(page_valid(page, slot_size));
#ifndef NDEBUG
     // In debug mode, write trash to freed memory
    for (uint32 i = 0; i < slot_size >> 3; i++) {
        ((uint64*)p)[i] = 0xbacafeeddeadbeef;
    }
#endif
     // Add to free list
    *(uint32*)p = page->first_free_slot;
    page->first_free_slot = (char*)p - (char*)page;
    page->bytes_used -= slot_size;
#ifdef UNI_LILAC_PROFILE
    uint32 sc = &first_partial - global.first_partial_pages;
    profiles[sc].slots_deallocated += 1;
    profiles[sc].slots_current -= 1;
#endif
     // It's possible to do a math trick to merge these branches into one, but
     // it ends up using more instructions even in the likely case.
    if (page->bytes_used == page_overhead) [[unlikely]] {
         // Page is empty, take it out of the partial list
        uint32 next = page->next_page;
        uint32 prev = page->prev_page;
        if (next) global.base[next].prev_page = prev;
        if (prev) global.base[prev].next_page = next;
        else first_partial = next;
         // And add it to the empty list.
        page->next_page = global.first_free_page;
        global.first_free_page = page - global.base;
#ifdef UNI_LILAC_PROFILE
        uint32 sc = &first_partial - global.first_partial_pages;
        profiles[sc].pages_emptied += 1;
        profiles[sc].pages_current -= 1;
#endif
    }
    else if (page->bytes_used + slot_size * 2 > page_size) [[unlikely]] {
         // Page went from full to partial, so put it on the partial list
        page->next_page = first_partial;
        page->prev_page = 0;
        uint32 here = page - global.base;
        if (page->next_page) {
            global.base[page->next_page].prev_page = here;
        }
        first_partial = here;
    }
}

void deallocate_large (void* p, usize) {
#ifdef UNI_LILAC_PROFILE
    oversize_deallocated += 1;
    oversize_current -= 1;
#endif
    std::free(p);
}

} // in

void dump_profile () {
#ifdef UNI_LILAC_PROFILE
    std::fprintf(stderr,
        "\ncl size page+ page- page= page> slot+ slot- slot= slot>\n");
    for (uint32 i = 0; i < in::n_size_classes; i++) {
        auto& p = in::profiles[i];
        std::fprintf(stderr, "%2u %4u %5zu %5zu %5u %5u %5zu %5zu %5u %5u\n",
            i, in::tables.class_sizes_d8[i] << 3,
            p.pages_picked, p.pages_emptied,
            p.pages_current, p.pages_most,
            p.slots_allocated, p.slots_deallocated,
            p.slots_current, p.slots_most
        );
    }
    std::fprintf(stderr, "over+ %zu over- %zu over= %zu over> %zu\n",
        in::oversize_allocated, in::oversize_deallocated,
        in::oversize_current, in::oversize_most
    );
    std::fprintf(stderr, "most pool used (no oversize): %u\n",
        (in::global.first_untouched_page - 1) * in::page_size
    );
//    uint8 counter = 1;
//    uint32 sum = 0;
//    for (auto b : in::decisions) {
//        sum += (b!=(counter > 1));
//        std::fputc(b!=(counter > 1)?'1':'0', stderr);
//        if (b && counter < 3) counter++;
//        else if (!b && counter > 0) counter--;
//    }
//    std::fputc('\n', stderr);
//    std::fprintf(stderr, "%u/%zu\n", sum, in::decisions.size());
#endif
}

} // uni::lilac
