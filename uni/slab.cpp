#include "slab.h"
#include "assertions.h"
#ifdef UNI_SLAB_PROFILE
#include "io.h"
#endif

namespace uni::slab::in {

struct alignas(slab_size) Slab {
    uint32 first_free_slot;
    uint32 bytes_used;
    uint32 next_slab;
    uint32 prev_slab;
    char data [slab_usable_size];
};
static_assert(sizeof(Slab) == slab_size);

 // Try to fit this in one cache line
struct alignas(64) Global {
     // Base points BEFORE the allocated arena, so that base[0] is INVALID and
     // base[1] is the first slab.
    Slab* base = null;
    uint32 first_free_slab = 0;
    uint32 first_untouched_slab = -1;
    uint32 first_partial_slabs [n_size_categories] = {};
};
static Global global;

#ifdef UNI_SLAB_PROFILE
struct Profile {
    usize slabs_picked = 0;
    usize slabs_emptied = 0;
    usize slots_allocated = 0;
    usize slots_deallocated = 0;
    uint32 slabs_current = 0;
    uint32 slabs_most = 0;
    uint32 slots_current = 0;
    uint32 slots_most = 0;
};
static Profile profiles [n_size_categories];
#endif

 // Noinline this and pass the argument along, so that internal_allocate can
 // tail call this.  This keeps internal_allocate from having to allocate a
 // stack frame to save cat during the call to std::aligned_alloc.
NOINLINE static
void* init_arena (uint32 cat) {
    void* arena = std::aligned_alloc(
        slab_size, allocation_limit
    );
    global.base = (Slab*)arena - 1;
    global.first_untouched_slab = 1;
    return internal_allocate(cat);
}

[[noreturn]] static
void out_of_memory () { require(false); abort(); }

NOINLINE
void* internal_allocate (uint32 cat) {
    expect(cat < n_size_categories);
    Slab* slab;
    if (!global.first_partial_slabs[cat]) {
         // Get new slab
        if (global.first_free_slab) {
             // Used previously freed slab
            slab = global.base + global.first_free_slab;
            global.first_partial_slabs[cat] = global.first_free_slab;
            global.first_free_slab = slab->next_slab;
        }
        else {
            if (global.first_untouched_slab > max_slabs) [[unlikely]] {
                if (!global.base) {
                     // We haven't actually initialized yet
                    return init_arena(cat);
                }
                else [[unlikely]] out_of_memory();
            }
             // Get fresh slab
            slab = global.base + global.first_untouched_slab;
            global.first_partial_slabs[cat] = global.first_untouched_slab;
            global.first_untouched_slab += 1;
        }
        slab->first_free_slot = 0;
        slab->bytes_used = 0;
        slab->next_slab = 0;
        slab->prev_slab = 0;
#ifdef UNI_SLAB_PROFILE
        profiles[cat].slabs_picked += 1;
        profiles[cat].slabs_current += 1;
        if (profiles[cat].slabs_most < profiles[cat].slabs_current)
            profiles[cat].slabs_most = profiles[cat].slabs_current;
#endif
    }
    else slab = global.base + global.first_partial_slabs[cat];
     // Check that slab header isn't corrupted
    expect(slab->first_free_slot < slab_size
        && slab->bytes_used <= slab_usable_size
        && slab->bytes_used % category_size(cat) == 0
    );
    void* r;
    if (slab->first_free_slot) {
        expect((slab->first_free_slot - 16) % category_size(cat) == 0);
         // Use previously freed slot
        r = (char*)slab + slab->first_free_slot;
        expect(*(uint32*)r < slab_size);
        slab->first_free_slot = *(uint32*)r;
    }
    else {
         // Use next untouched slot.  If there are no freed slots, then
         // bytes_used happens to also be the offset of the next untouched slot.
        r = slab->data + slab->bytes_used;
    }
    slab->bytes_used += category_size(cat);
#ifdef UNI_SLAB_PROFILE
    profiles[cat].slots_allocated += 1;
    profiles[cat].slots_current += 1;
    if (profiles[cat].slots_most < profiles[cat].slots_current)
        profiles[cat].slots_most = profiles[cat].slots_current;
#endif
    if (slab->bytes_used > slab_usable_size - category_size(cat)) {
         // Slab is full!  Take it out of the partial list.
        if (slab->next_slab) {
            Slab* next = global.base + slab->next_slab;
            next->prev_slab = slab->prev_slab;
            slab->next_slab = 0;  // Not really needed...
        }
        if (slab->prev_slab) {
            Slab* prev = global.base + slab->prev_slab;
            prev->next_slab = slab->next_slab;
            slab->prev_slab = 0;
        }
        else global.first_partial_slabs[cat] = slab->next_slab;
    }
    return r;
}

void internal_deallocate (void* p, uint32 cat) {
    expect(cat < n_size_categories);
     // Check that we own this pointer
    expect((char*)p > (char*)(global.base + 1)
        && (char*)p < (char*)(global.base + global.first_untouched_slab)
    );
    usize address = (usize)p;
     // Check that pointer is aligned correctly
    expect((address % slab_size - slab_overhead) % category_size(cat) == 0);
    Slab* slab = (Slab*)(address & ~(slab_size - 1));
     // Check that slab header isn't corrupted
    expect(slab->first_free_slot < slab_size
        && slab->bytes_used <= slab_usable_size
        && slab->bytes_used % category_size(cat) == 0
    );
    *(uint32*)p = slab->first_free_slot;
    slab->first_free_slot = (char*)p - (char*)slab;
    slab->bytes_used -= category_size(cat);
#ifdef UNI_SLAB_PROFILE
    profiles[cat].slots_deallocated += 1;
    profiles[cat].slots_current -= 1;
#endif
    if (slab->bytes_used == 0) {
         // Slab is empty, take it out of the partial list and add it to the
         // empty list
        if (slab->next_slab) {
            Slab* next = global.base + slab->next_slab;
            next->prev_slab = slab->prev_slab;
        }
        if (slab->prev_slab) {
            Slab* prev = global.base + slab->prev_slab;
            prev->next_slab = slab->next_slab;
            slab->prev_slab = 0;
        }
        else global.first_partial_slabs[cat] = slab->next_slab;
        slab->next_slab = global.first_free_slab;
        global.first_free_slab = slab - global.base;
#ifdef UNI_SLAB_PROFILE
        profiles[cat].slabs_emptied += 1;
        profiles[cat].slabs_current -= 1;
#endif
    }
    else if (slab->bytes_used > slab_usable_size - category_size(cat) * 2) {
         // Slab went from full to partial, so put it on the partial list
        uint32 here = slab - global.base;
        slab->next_slab = global.first_partial_slabs[cat];
        if (slab->next_slab) {
            global.base[slab->next_slab].prev_slab = here;
        }
        global.first_partial_slabs[cat] = here;
    }
}

static_assert(size_category(0) == 0);
static_assert(size_category(7) == 0);
static_assert(size_category(8) == 0);
static_assert(size_category(9) == 1);
static_assert(size_category(15) == 1);
static_assert(size_category(16) == 1);
static_assert(size_category(17) == 2);
static_assert(size_category(23) == 2);
static_assert(size_category(24) == 2);
static_assert(size_category(25) == 3);
static_assert(size_category(47) == 5);
static_assert(size_category(48) == 5);
static_assert(size_category(49) == 6);
static_assert(size_category(56) == 6);
static_assert(category_size(0) == 8);
static_assert(category_size(1) == 16);
static_assert(category_size(2) == 24);
static_assert(category_size(3) == 32);
static_assert(category_size(4) == 40);
static_assert(category_size(5) == 48);
static_assert(category_size(6) == 56);

} // uni::slab::in

#ifdef UNI_SLAB_PROFILE
namespace uni::slab {
void dump_profile () {
    warn_utf8("cat size sb+ sb- sb= sb> st+ st- st= st>\n");
    for (usize i = 0; i < n_size_categories; i++) {
        auto& p = in::profiles[i];
        warn_utf8(cat(
            i, ' ', in::category_size(i),
            ' ', p.slabs_picked, ' ', p.slabs_emptied,
            ' ', p.slabs_current, ' ', p.slabs_most,
            ' ', p.slots_allocated, ' ', p.slots_deallocated,
            ' ', p.slots_current, ' ', p.slots_most, '\n'
        ));
    }
    warn_utf8(cat("most slabs used: ", in::global.first_untouched_slab - 1, '\n'));
}
} // uni::slab
#endif
