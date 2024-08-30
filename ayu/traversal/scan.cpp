#include "scan.h"

#include "../reflection/anyptr.h"
#include "../reflection/anyref.h"
#include "../reflection/descriptors.private.h"
#include "../resources/universe.private.h"
#include "compound.h"
#include "location.h"
#include "traversal.private.h"

namespace ayu {
namespace in {

struct ScanContext;

struct ScanTraversalHead {
    ScanContext* context;
    LocationRef loc;
};

template <class T = Traversal>
struct ScanTraversal : ScanTraversalHead, T { };

struct ScanContext {
    CallbackRef<bool(const ScanTraversal<>&)> cb;
    bool done = false;
};

struct TraverseScan {

    static
    bool start_pointers (
        AnyPtr base_item, LocationRef base_loc,
        CallbackRef<bool(AnyPtr, LocationRef)> cb
    ) {
        if (currently_scanning) {
            raise(e_ScanWhileScanning,
                "Cannot start scan while there's already a scan running."
            );
        }
        currently_scanning = true;
        ScanContext ctx {
            CallbackRef<bool(const ScanTraversal<>&)>(
                cb, [](auto& cb, const ScanTraversal<>& trav) -> bool {
                    return trav.addressable &&
                        cb(AnyPtr(trav.desc, trav.address), trav.loc);
                }
            )
        };
        ScanTraversal<StartTraversal> child;
        child.context = &ctx;
        child.loc = base_loc;
        trav_start<visit>(child, base_item, base_loc, true, AccessMode::Read);
        currently_scanning = false;
        return ctx.done;
    }

    static
    bool start_references (
        const AnyRef& base_item, LocationRef base_loc,
        CallbackRef<bool(const AnyRef&, LocationRef)> cb
    ) {
        ScanContext ctx {
            CallbackRef<bool(const ScanTraversal<>&)>(
                cb, [](auto& cb, const ScanTraversal<>& trav) -> bool{
                    return cb(trav.to_reference(), trav.loc);
                }
            )
        };
        ScanTraversal<StartTraversal> child;
        child.context = &ctx;
        child.loc = base_loc;
        trav_start<visit>(child, base_item, base_loc, false, AccessMode::Read);
        currently_scanning = false;
        return ctx.done;
    }

    static
    bool scan_item (const ScanTraversal<>& trav) {
        return trav.context->done = trav.context->cb(trav);
    }

    NOINLINE static
    void visit (const Traversal& tr) {
        auto& trav = static_cast<const ScanTraversal<>&>(tr);
         // Although we always call the callback first, doing so requires saving
         // and restoring all our arguments, which also prevents tail calls.  So
         // we're doing it at the top of each execution function instead of at
         // the top of this decision function.  The callback is expected to
         // return true either rarely or never, so it's okay to delay checking
         // its return a bit.
         //
         // Also we're only checking offsets here, not converting them to
         // variables, because doing so before calling the cb would require
         // saving and restoring those variables.
         //
         // TODO: These might no longer be the case with the new traversal
         // system.
        if (!!(trav.desc->type_flags & TypeFlags::NoRefsToChildren)) {
             // Wait, never mind, don't scan under this.
            scan_item(trav);
        }
        else if (trav.desc->preference() == DescFlags::PreferObject) {
            if (trav.desc->keys_offset) {
                use_computed_attrs(trav);
            }
            else use_attrs(trav);
        }
        else if (trav.desc->preference() == DescFlags::PreferArray) {
            if (trav.desc->length_offset) {
                if (!!(trav.desc->flags & DescFlags::ElemsContiguous)) {
                    use_contiguous_elems(trav);
                }
                else use_computed_elems(trav);
            }
            else use_elems(trav);
        }
        else if (trav.desc->delegate_offset) {
            return use_delegate(trav);
        }
         // Down here, we aren't using the arguments any more, so the compiler
         // doesn't need to save them and we can tail call the cb
        else scan_item(trav);
    }

    NOINLINE static
    void use_attrs (const ScanTraversal<>& trav) {
        if (scan_item(trav)) [[unlikely]] return;
        expect(trav.desc->attrs_offset);
        auto attrs = trav.desc->attrs();
        for (uint i = 0; i < attrs->n_attrs; i++) {
            auto attr = attrs->attr(i);
             // Not discarding invisible attrs for scan purposes.
            auto acr = attr->acr();
            SharedLocation child_loc;
             // TODO: verify that the child item is object-like.
            ScanTraversal<AttrTraversal> child;
            child.context = trav.context;
            if (!!(acr->attr_flags & AttrFlags::Include)) {
                 // Behave as though all included attrs are included (collapse
                 // the location segment for the included attr).
                child.loc = trav.loc;
            }
            else {
                child_loc = SharedLocation(trav.loc, attr->key);
                child.loc = child_loc;
            }
            trav_attr<visit>(child, trav, acr, attr->key, AccessMode::Read);
            if (child.context->done) [[unlikely]] return;
        }
    }

    NOINLINE static
    void use_computed_attrs (const ScanTraversal<>& trav) {
        if (scan_item(trav)) [[unlikely]] return;
        expect(trav.desc->keys_offset);
        auto keys_acr = trav.desc->keys_acr();
         // Get list of keys
        AnyArray<AnyString> keys;
         // TODO: better
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
            auto child_loc = SharedLocation(trav.loc, key);
            ScanTraversal<ComputedAttrTraversal> child;
            child.context = trav.context;
            child.loc = child_loc;
            trav_computed_attr<visit>(
                child, trav, ref, f, key, AccessMode::Read
            );
            if (child.context->done) [[unlikely]] return;
        }
    }

    NOINLINE static
    void use_elems (const ScanTraversal<>& trav) {
        if (scan_item(trav)) [[unlikely]] return;
        expect(trav.desc->elems_offset);
        auto elems = trav.desc->elems();
        for (uint i = 0; i < elems->n_elems; i++) {
            auto elem = elems->elem(i);
            auto acr = elem->acr();
            SharedLocation child_loc;
            ScanTraversal<ElemTraversal> child;
            child.context = trav.context;
            if (trav.collapse_optional) {
                 // It'd be weird to specify collapse_optional when the child
                 // item uses non-computed elems, but it's valid.  TODO: assert
                 // that there aren't multiple elems?
                child.loc = trav.loc;
            }
            else {
                child_loc = SharedLocation(trav.loc, i);
                child.loc = child_loc;
            }
             // TODO: verify that the child item is array-like.
            trav_elem<visit>(child, trav, acr, i, AccessMode::Read);
            if (child.context->done) [[unlikely]] return;
        }
    }

    NOINLINE static
    void use_computed_elems (const ScanTraversal<>& trav) {
        if (scan_item(trav)) [[unlikely]] return;
        expect(trav.desc->length_offset);
         // TODO: deduplicate
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
            SharedLocation child_loc;
            ScanTraversal<ComputedElemTraversal> child;
            child.context = trav.context;
            if (trav.collapse_optional) {
                child.loc = trav.loc;
            }
            else {
                child_loc = SharedLocation(trav.loc, i);
                child.loc = child_loc;
            }
            trav_computed_elem<visit>(
                child, trav, ref, f, i, AccessMode::Read
            );
            if (child.context->done) [[unlikely]] return;
        }
    }

    NOINLINE static
    void use_contiguous_elems (const ScanTraversal<>& trav) {
        if (scan_item(trav)) [[unlikely]] return;
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
            for (usize i = 0; i < len; i++) {
                SharedLocation child_loc;
                ScanTraversal<ContiguousElemTraversal> child;
                child.context = trav.context;
                if (trav.collapse_optional) {
                    child.loc = trav.loc;
                }
                else {
                    child_loc = SharedLocation(trav.loc, i);
                    child.loc = child_loc;
                }
                trav_contiguous_elem<visit>(
                    child, trav, ptr, f, i, AccessMode::Read
                );
                if (child.context->done) [[unlikely]] return;
                ptr.address = (Mu*)((char*)ptr.address + ptr.type.cpp_size());
            }
        }
    }

    NOINLINE static
    void use_delegate (const ScanTraversal<>& trav) {
        if (scan_item(trav)) [[unlikely]] return;
        ScanTraversal<DelegateTraversal> child;
        child.context = trav.context;
        child.loc = trav.loc;
        expect(trav.desc->delegate_offset);
        auto acr = trav.desc->delegate_acr();
        trav_delegate<visit>(child, trav, acr, AccessMode::Read);
    }
};

 // Store a typed AnyPtr instead of a Mu* because items at the same address
 // with different types are different items.
static UniqueArray<Pair<AnyPtr, SharedLocation>> location_cache;
static bool have_location_cache = false;
static usize keep_location_cache_count = 0;
bool get_location_cache () {
    if (!keep_location_cache_count) return false;
    if (!have_location_cache) {
        plog("Generate location cache begin");
        scan_universe_pointers([](AnyPtr ptr, LocationRef loc){
             // We're deliberately ignoring the case where the same typed
             // pointer turns up twice in the data tree.  If this happens, we're
             // probably dealing with some sort of shared_ptr-like situation,
             // and in that case it shouldn't matter which location gets cached.
             // It could theoretically be a problem if the pointers differ in
             // readonlyness, but that should probably never happen.
            location_cache.emplace_back(ptr, loc);
            return false;
        });
        plog("Generate location cache sort");
        std::sort(location_cache.begin(), location_cache.end(),
            [](const auto& a, const auto& b){ return a.first < b.first; }
        );
        have_location_cache = true;
        plog("Generate location cache end");
#ifdef AYU_PROFILE
        fprintf(stderr, "Location cache entries: %ld\n", location_cache.size());
#endif
    }
    return true;
}

const Pair<AnyPtr, SharedLocation>* search_location_cache (AnyPtr item) {
    if (!have_location_cache) return null;
    auto bottom = location_cache.begin();
    auto top = location_cache.end();
    while (top != bottom) {
        auto mid = bottom + (top - bottom) / 2;
        if (mid->first == item) return mid;
        if (mid->first < item) bottom = mid;
        else top = mid;
    }
    return null;
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

static PushLikelyRef* first_plr = null;

PushLikelyRef::PushLikelyRef (
    AnyRef r, MoveRef<SharedLocation> l
) noexcept :
    reference(r), location(*move(l)), next(first_plr)
{
#ifndef NDEBUG
    require(reference_from_location(location) == reference);
#endif
    first_plr = this;
}
PushLikelyRef::~PushLikelyRef () { first_plr = next; }

bool scan_pointers (
    AnyPtr base_item, LocationRef base_loc,
    CallbackRef<bool(AnyPtr, LocationRef)> cb
) {
    return TraverseScan::start_pointers(base_item, base_loc, cb);
}

bool scan_references (
    const AnyRef& base_item, LocationRef base_loc,
    CallbackRef<bool(const AnyRef&, LocationRef)> cb
) {
    return TraverseScan::start_references(base_item, base_loc, cb);
}

bool scan_resource_pointers (
    ResourceRef res, CallbackRef<bool(AnyPtr, LocationRef)> cb
) {
    auto& value = res->get_value();
    if (!value) return false;
    return scan_pointers(value.ptr(), SharedLocation(res), cb);
}

bool scan_resource_references (
    ResourceRef res, CallbackRef<bool(const AnyRef&, LocationRef)> cb
) {
    auto& value = res->get_value();
    if (!value) return false;
    return scan_references(value.ptr(), SharedLocation(res), cb);
}

bool scan_universe_pointers (
    CallbackRef<bool(AnyPtr, LocationRef)> cb
) {
    if (auto loc = current_base_location()) {
        if (auto ref = loc->reference()) {
            if (auto address = ref->address()) {
               scan_pointers(AnyPtr(ref->type(), address), loc, cb);
            }
        }
    }
    for (auto& [_, res] : universe().resources) {
        if (scan_resource_pointers(res, cb)) return true;
    }
    return false;
}

bool scan_universe_references (
    CallbackRef<bool(const AnyRef&, LocationRef)> cb
) {
     // To allow serializing self-referential data structures that aren't inside
     // a Resource, first scan the currently-being-serialized item, but only if
     // it's not in a Resource (so we don't duplicate work).
     // TODO: Maybe don't do this if the traversal was started by a scan,
     // instead of by a serialize.
    if (auto loc = current_base_location()) {
        if (auto ref = loc->reference()) {
            if (scan_references(*ref, loc, cb)) {
                return true;
            }
        }
    }
    for (auto& [_, res] : universe().resources) {
        if (scan_resource_references(res, cb)) return true;
    }
    return false;
}

SharedLocation find_pointer (AnyPtr item) {
    if (!item) return {};
    for (auto plr = first_plr; plr; plr = plr->next) {
        if (AnyRef(item) == plr->reference) return plr->location;
    }
    if (get_location_cache()) {
        if (auto it = search_location_cache(item)) {
             // Reject non-readonly pointer to readonly location
            if (it->first.readonly() && !item.readonly()) {
                [[unlikely]] return {};
            }
            return it->second;
        }
        return {};
    }
    else {
        SharedLocation r;
        scan_universe_pointers([&r, item](AnyPtr p, LocationRef loc){
            if (p == item) {
                 // If we get a non-readonly pointer to a readonly location,
                 // reject it, but also don't keep searching.
                if (p.readonly() && !item.readonly()) [[unlikely]] return true;
                new (&r) SharedLocation(loc);
                return true;
            }
            return false;
        });
        return r;
    }
}

SharedLocation find_reference (const AnyRef& item) {
    if (!item) return {};
    for (auto plr = first_plr; plr; plr = plr->next) {
        if (item == plr->reference) return plr->location;
    }
    if (get_location_cache()) {
        if (Mu* address = item.address()) {
             // Addressable! This will be fast.
            auto ptr = AnyPtr(item.type(), address);
            if (auto it = search_location_cache(ptr)) {
                if (it->first.readonly() && !item.readonly()) {
                    [[unlikely]] return {};
                }
                return it->second;
            }
            return {};
        }
        else {
             // Not addressable.  First find the host in the location cache.
            if (auto it = search_location_cache(item.host)) {
                 // Now search under that host for the actual reference.
                SharedLocation r;
                scan_references(
                    AnyRef(item.host), it->second,
                    [&r, &item](const AnyRef& ref, LocationRef loc)
                {
                    if (ref == item) {
                        if (ref.readonly() && !item.readonly()) {
                            [[unlikely]] return true;
                        }
                        new (&r) SharedLocation(loc);
                        return true;
                    }
                    else return false;
                });
                return r;
            }
            else return {};
        }
    }
    else {
         // We don't have the location cache!  Time to do a global search.
        SharedLocation r;
        scan_universe_references(
            [&r, &item](const AnyRef& ref, LocationRef loc)
        {
            if (ref == item) {
                if (ref.readonly() && !item.readonly()) {
                    [[unlikely]] return true;
                }
                new (&r) SharedLocation(loc);
                return true;
            }
            else return false;
        });
        return r;
    }
}

SharedLocation pointer_to_location (AnyPtr item) {
    if (!item) return {};
    else if (SharedLocation r = find_pointer(item)) {
        return r;
    }
    else raise(e_ReferenceNotFound, cat(
        "Couldn't locate pointer target of type ", item.type.name()
    ));
}

SharedLocation reference_to_location (const AnyRef& item) {
    if (!item) return {};
    else if (SharedLocation r = find_reference(item)) {
        return r;
    }
    else raise(e_ReferenceNotFound, cat(
        "Couldn't locate reference target of type ", item.type().name()
    ));
}

bool currently_scanning = false;

} using namespace ayu;
