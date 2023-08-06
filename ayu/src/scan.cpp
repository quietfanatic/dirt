#include "../scan.h"

#include "../location.h"
#include "../pointer.h"
#include "../reference.h"
#include "descriptors-private.h"
#include "serialize-compound-private.h"
#include "traversal-private.h"
#include "universe-private.h"

namespace ayu {
namespace in {

bool scan_trav (
    const Traversal& trav, LocationRef loc,
    CallbackRef<bool(const Traversal&, LocationRef)> cb
) {
    if (cb(trav, loc)) return true;
    switch (trav.desc->preference()) {
        case Description::PREFER_OBJECT: {
            UniqueArray<AnyString> ks;
            ser_collect_keys(trav, ks);
            for (auto& k : ks) {
                 // Initialize to false because in only_addressable mode, the
                 // callback may not be called.
                bool r = false;
                ser_attr(trav, k, AccessMode::Read,
                    [&r, loc, &k, &cb](const Traversal& child)
                {
                    r = scan_trav(child, Location(loc, k), cb);
                });
                if (r) return true;
            }
            return false;
        }
        case Description::PREFER_ARRAY: {
            usize len = ser_get_length(trav);
            for (usize i = 0; i < len; i++) {
                bool r = false;
                ser_elem(trav, i, AccessMode::Read,
                    [&r, loc, i, &cb](const Traversal& child)
                {
                    r = scan_trav(child, Location(loc, i), cb);
                });
                if (r) return true;
            }
            return false;
        }
        default: {
            if (auto acr = trav.desc->delegate_acr()) {
                bool r = false;
                trav.follow_delegate(acr, AccessMode::Read,
                    [&r, loc, &cb](const Traversal& child)
                {
                    r = scan_trav(child, loc, cb);
                });
                return r;
            }
            return false;
        }
    }
}

 // Store a typed Pointer instead of a Mu* because items at the same address
 // with different types are different items.
static std::unordered_map<Pointer, Location> location_cache;
static bool have_location_cache = false;
static usize keep_location_cache_count = 0;
std::unordered_map<Pointer, Location>* get_location_cache () {
    if (!keep_location_cache_count) return null;
    if (!have_location_cache) {
        scan_universe_pointers([](Pointer ptr, LocationRef loc){
             // We're deliberately ignoring the case where the same typed
             // pointer turns up twice in the data tree.  If this happens, we're
             // probably dealing with some sort of shared_ptr-like situation,
             // and in that case it shouldn't matter which location gets cached.
             // It could theoretically be a problem if the pointers differ in
             // readonlyness, but that should probably never happen.
            location_cache.emplace(ptr, loc);
            return false;
        });
        have_location_cache = true;
    }
    return &location_cache;
}

} using namespace in;

KeepLocationCache::KeepLocationCache () noexcept {
    keep_location_cache_count++;
}
KeepLocationCache::~KeepLocationCache () {
    if (!--keep_location_cache_count) {
        have_location_cache = false;
        location_cache.clear();
    }
}

bool scan_pointers (
    Pointer base_item, LocationRef base_loc,
    CallbackRef<bool(Pointer, LocationRef)> cb
) {
    bool r = false;
    Traversal::start(base_item, base_loc, true, AccessMode::Read,
        [&r, base_loc, cb](const Traversal& trav)
    {
        r = scan_trav(
            trav, base_loc, [cb](const Traversal& trav, LocationRef loc)
        {
            if (trav.addressable) {
                return cb(Pointer(trav.desc, trav.address), loc);
            }
            else return false;
        });
    });
    return r;
}

bool scan_references (
    const Reference& base_item, LocationRef base_loc,
    CallbackRef<bool(const Reference&, LocationRef)> cb
) {
    bool r = false;
    Traversal::start(base_item, base_loc, false, AccessMode::Read,
        [&r, base_loc, cb](const Traversal& trav)
    {
        r = scan_trav(
            trav, base_loc, [cb](const Traversal& trav, LocationRef loc)
        {
            return cb(trav.to_reference(), loc);
        });
    });
    return r;
}

bool scan_resource_pointers (
    const Resource& res, CallbackRef<bool(Pointer, LocationRef)> cb
) {
    if (res.state() == UNLOADED) return false;
    return scan_pointers(res.get_value().ptr(), Location(res), cb);
}
bool scan_resource_references (
    const Resource& res, CallbackRef<bool(const Reference&, LocationRef)> cb
) {
    if (res.state() == UNLOADED) return false;
    return scan_references(res.get_value().ptr(), Location(res), cb);
}

bool scan_universe_pointers (
    CallbackRef<bool(Pointer, LocationRef)> cb
) {
    for (auto& [_, resdat] : universe().resources) {
        if (scan_resource_pointers(&*resdat, cb)) return true;
    }
    return false;
}

bool scan_universe_references (
    CallbackRef<bool(const Reference&, LocationRef)> cb
) {
     // To allow serializing self-referential data structures that aren't inside
     // a Resource, first scan the currently-being-serialized item, but only if
     // it's not in a Resource (so we don't duplicate work).
     // TODO: Maybe don't do this if the traversal was started by a scan,
     // instead of by a serialize.
    if (Location loc = current_base_location()) {
        if (auto ref = loc.reference()) {
            if (scan_references(*ref, loc, cb)) {
                return true;
            }
        }
    }
    for (auto& [_, resdat] : universe().resources) {
        if (scan_resource_references(&*resdat, cb)) return true;
    }
    return false;
}

Location find_pointer (Pointer item) {
    if (!item) return Location();
    else if (auto cache = get_location_cache()) {
        auto it = cache->find(item);
        if (it != cache->end()) {
             // Reject non-readonly pointer to readonly location
            if (it->first.readonly() && !item.readonly()) {
                [[unlikely]] return Location();
            }
            return it->second;
        }
        return Location();
    }
    else {
        Location r;
        scan_universe_pointers([&r, item](Pointer p, LocationRef loc){
            if (p == item) {
                 // If we get a non-readonly pointer to a readonly location,
                 // reject it, but also don't keep searching.
                if (p.readonly() && !item.readonly()) [[unlikely]] return true;
                new (&r) Location(loc);
                return true;
            }
            return false;
        });
        return r;
    }
}

Location find_reference (const Reference& item) {
    if (!item) return Location();
    else if (auto cache = get_location_cache()) {
        if (Mu* address = item.address()) {
             // Addressable! This will be fast.
            auto it = cache->find(Pointer(item.type(), address));
            if (it != cache->end()) {
                if (it->first.readonly() && !item.readonly()) {
                    [[unlikely]] return Location();
                }
                return it->second;
            }
            return Location();
        }
        else {
             // Not addressable.  First find the host in the location cache.
            auto it = cache->find(item.host);
            if (it != cache->end()) {
                 // Now search under that host for the actual reference.
                 // This will likely fail because it's hard to compare
                 // unaddressable references, but try anyway.
                Location r;
                scan_references(
                    Reference(item.host), it->second,
                    [&r, &item](const Reference& ref, LocationRef loc)
                {
                    if (ref == item) {
                        if (ref.readonly() && !item.readonly()) {
                            [[unlikely]] return true;
                        }
                        new (&r) Location(loc);
                        return true;
                    }
                    else return false;
                });
                return r;
            }
            else return Location();
        }
    }
    else {
         // We don't have the location cache!  Time to do a global search.
        Location r;
        scan_universe_references(
            [&r, &item](const Reference& ref, LocationRef loc)
        {
            if (ref == item) {
                if (ref.readonly() && !item.readonly()) {
                    [[unlikely]] return true;
                }
                new (&r) Location(loc);
                return true;
            }
            else return false;
        });
        return r;
    }
}

Location pointer_to_location (Pointer item) {
    if (!item) return Location();
    else if (Location r = find_pointer(item)) {
        return r;
    }
    else raise(e_ReferenceNotFound, cat(
        "Couldn't locate pointer target of type ", item.type.name()
    ));
}
Location reference_to_location (const Reference& item) {
    if (!item) return Location();
    else if (Location r = find_reference(item)) {
        return r;
    }
    else raise(e_ReferenceNotFound, cat(
        "Couldn't locate reference target of type ", item.type().name()
    ));
}

} using namespace ayu;
