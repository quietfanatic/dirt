#include "scan.h"

#include "../reflection/descriptors.private.h"
#include "../reflection/pointer.h"
#include "../reflection/reference.h"
#include "../resources/universe.private.h"
#include "compound.h"
#include "location.h"
#include "traversal.private.h"

namespace ayu {
namespace in {

struct TraverseScan {

    static
    bool start_pointers (
        Pointer base_item, LocationRef base_loc,
        CallbackRef<bool(Pointer, LocationRef)> cb
    ) {
        bool r = false;
        trav_start(base_item, base_loc, true, AccessMode::Read,
            [&r, base_loc, cb](const Traversal& trav)
        {
            r = traverse(trav, base_loc,
                [cb](const Traversal& trav, LocationRef loc)
            {
                return trav.addressable &&
                    cb(Pointer(trav.desc, trav.address), loc);
            });
        });
        return r;
    }

    static
    bool start_references (
        const Reference& base_item, LocationRef base_loc,
        CallbackRef<bool(const Reference&, LocationRef)> cb
    ) {
        bool r = false;
        trav_start(base_item, base_loc, false, AccessMode::Read,
            [&r, base_loc, cb](const Traversal& trav)
        {
            r = traverse(trav, base_loc,
                [cb](const Traversal& trav, LocationRef loc)
            { return cb(trav.to_reference(), loc); });
        });
        return r;
    }

    NOINLINE static
    bool traverse (
        const Traversal& trav, LocationRef loc,
        CallbackRef<bool(const Traversal&, LocationRef)> cb
    ) {
         // Although we always call cb(trav, loc) first, doing so requires
         // saving and restoring all our arguments, which also prevents tail
         // calls.  So we're doing it at the top of each execution function
         // instead of at the top of this decision function.  The callback is
         // expected to return true either rarely or never, so it's okay to
         // delay checking its return a bit.
         //
         // Also we're only checking offsets here, not converting them to
         // variables, because doing so before calling the cb would require
         // saving and restoring those variables.
        if (trav.desc->preference() == DescFlags::PreferObject) {
            if (trav.desc->keys_offset) {
                return use_computed_attrs(trav, loc, cb);
            }
            else {
                return use_attrs(trav, loc, cb);
            }
        }
        else if (trav.desc->preference() == DescFlags::PreferArray) {
            if (trav.desc->length_offset) {
                if (!!(trav.desc->flags & DescFlags::ElemsContiguous)) {
                    return use_contiguous_elems(trav, loc, cb);
                }
                else {
                    return use_computed_elems(trav, loc, cb);
                }
            }
            else {
                return use_elems(trav, loc, cb);
            }
        }
        else if (trav.desc->delegate_offset) {
            return use_delegate(trav, loc, cb);
        }
         // Down here, we aren't using the arguments any more, so the compiler
         // doesn't need to save them and we can tail call the cb
        else return cb(trav, loc);
    }

    NOINLINE static
    bool use_attrs (
        const Traversal& trav, LocationRef loc,
        CallbackRef<bool(const Traversal&, LocationRef)> cb
    ) {
        if (cb(trav, loc)) return true;
        expect(trav.desc->attrs_offset);
        auto attrs = trav.desc->attrs();
        for (uint i = 0; i < attrs->n_attrs; i++) {
            auto attr = attrs->attr(i);
             // Not discarding invisible attrs for scan purposes.
            auto acr = attr->acr();
             // Behave as though all included attrs are included (collapse the
             // location segment for the included attr).
            Location child_loc =
                !!(acr->attr_flags & AttrFlags::Include)
                ? *loc : Location(loc, attr->key);
             // TODO: verify that the child item is object-like.
            bool r = false; // init in case only_addressable skips cb
            trav_attr(trav, acr, attr->key, AccessMode::Read,
                [&r, child_loc, cb](const Traversal& child)
            { r = traverse(child, child_loc, cb); });
            if (r) return true;
        }
        return false;
    }

    NOINLINE static
    bool use_computed_attrs (
        const Traversal& trav, LocationRef loc,
        CallbackRef<bool(const Traversal&, LocationRef)> cb
    ) {
        if (cb(trav, loc)) return true;
        expect(trav.desc->keys_offset);
        auto keys_acr = trav.desc->keys_acr();
         // Get list of keys
        AnyArray<AnyString> keys;
        keys_acr->read(*trav.address, [&keys](Mu& v){
            new (&keys) AnyArray<AnyString>(
                reinterpret_cast<const AnyArray<AnyString>&>(v)
            );
        });
        expect(trav.desc->computed_attrs_offset);
        auto f = trav.desc->computed_attrs()->f;
         // Now scan for each key
        for (auto& key : keys) {
            auto ref = f(*trav.address, key);
            if (!ref) raise_AttrNotFound(trav.desc, key);
            Location child_loc = Location(loc, key);
            bool r = false;
            trav_computed_attr(trav, ref, f, key, AccessMode::Read,
                [&r, child_loc, cb](const Traversal& child)
            { r = traverse(child, child_loc, cb); });
            if (r) return true;
        }
        return false;
    }

    NOINLINE static
    bool use_elems (
        const Traversal& trav, LocationRef loc,
        CallbackRef<bool(const Traversal&, LocationRef)> cb
    ) {
        if (cb(trav, loc)) return true;
        expect(trav.desc->elems_offset);
        auto elems = trav.desc->elems();
        for (uint i = 0; i < elems->n_elems; i++) {
            auto elem = elems->elem(i);
            auto acr = elem->acr();
             // It'd be weird to specify collapse_optional for non-computed
             // elems, but it's valid.
            Location child_loc =
                trav.collapse_optional ? *loc : Location(loc, i);
             // TODO: verify that the child item is array-like.
            bool r = false;
            trav_elem(trav, acr, i, AccessMode::Read,
                [&r, child_loc, cb](const Traversal& child)
            { r = traverse(child, child_loc, cb); });
            if (r) return true;
        }
        return false;
    }

    NOINLINE static
    bool use_contiguous_elems (
        const Traversal& trav, LocationRef loc,
        CallbackRef<bool(const Traversal&, LocationRef)> cb
    ) {
        if (cb(trav, loc)) return true;
        expect(trav.desc->length_offset);
        auto length_acr = trav.desc->length_acr();
        usize len;
        length_acr->read(*trav.address, [&len](Mu& v){
            len = reinterpret_cast<usize&>(v);
        });
        if (len) {
            expect(trav.desc->contiguous_elems_offset);
            auto f = trav.desc->contiguous_elems()->f;
            auto ptr = f(*trav.address);
            auto child_desc = DescriptionPrivate::get(ptr.type);
            for (usize i = 0; i < len; i++) {
                Location child_loc =
                    trav.collapse_optional ? *loc : Location(loc, i);
                bool r = false;
                trav_contiguous_elem(trav, ptr, f, i, AccessMode::Read,
                    [&r, child_loc, cb](const Traversal& child)
                { r = traverse(child, child_loc, cb); });
                if (r) return true;
                ptr.address = (Mu*)((char*)ptr.address + child_desc->cpp_size);
            }
        }
        return false;
    }

    NOINLINE static
    bool use_computed_elems (
        const Traversal& trav, LocationRef loc,
        CallbackRef<bool(const Traversal&, LocationRef)> cb
    ) {
        if (cb(trav, loc)) return true;
        expect(trav.desc->length_offset);
        auto length_acr = trav.desc->length_acr();
        usize len;
        length_acr->read(*trav.address, [&len](Mu& v){
            len = reinterpret_cast<usize&>(v);
        });
        expect(trav.desc->computed_elems_offset);
        auto f = trav.desc->computed_elems()->f;
        for (usize i = 0; i < len; i++) {
            auto ref = f(*trav.address, i);
            if (!ref) raise_ElemNotFound(trav.desc, i);
            Location child_loc =
                trav.collapse_optional ? *loc : Location(loc, i);
            bool r = false;
            trav_computed_elem(trav, ref, f, i, AccessMode::Read,
                [&r, child_loc, cb](const Traversal& child)
            { r = traverse(child, child_loc, cb); });
            if (r) return true;
        }
        return false;
    }

    NOINLINE static
    bool use_delegate (
        const Traversal& trav, LocationRef loc,
        CallbackRef<bool(const Traversal&, LocationRef)> cb
    ) {
        if (cb(trav, loc)) return true;
        expect(trav.desc->delegate_offset);
        auto acr = trav.desc->delegate_acr();
        bool r = false;
        trav_delegate(trav, acr, AccessMode::Read,
            [&r, loc, cb](const Traversal& child)
        { r = traverse(child, loc, cb); });
        return r;
    }
};

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

static PushLikelyReference* first_plr = null;

PushLikelyReference::PushLikelyReference (Reference r, Location l) noexcept :
    reference(r), location(l), next(first_plr)
{
#ifndef NDEBUG
    require(reference_from_location(location) == reference);
#endif
    first_plr = this;
}
PushLikelyReference::~PushLikelyReference () { first_plr = next; }

bool scan_pointers (
    Pointer base_item, LocationRef base_loc,
    CallbackRef<bool(Pointer, LocationRef)> cb
) {
    return TraverseScan::start_pointers(base_item, base_loc, cb);
}

bool scan_references (
    const Reference& base_item, LocationRef base_loc,
    CallbackRef<bool(const Reference&, LocationRef)> cb
) {
    return TraverseScan::start_references(base_item, base_loc, cb);
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
    if (Location loc = current_base_location()) {
        if (auto ref = loc.reference()) {
            if (auto address = ref->address()) {
               scan_pointers(Pointer(ref->type(), address), loc, cb);
            }
        }
    }
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
    for (auto plr = first_plr; plr; plr = plr->next) {
        if (Reference(item) == plr->reference) return plr->location;
    }
    if (auto cache = get_location_cache()) {
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
    for (auto plr = first_plr; plr; plr = plr->next) {
        if (item == plr->reference) return plr->location;
    }
    if (auto cache = get_location_cache()) {
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
