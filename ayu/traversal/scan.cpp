#include "scan.h"
#include <algorithm>
#include "../reflection/anyptr.h"
#include "../reflection/anyref.h"
#include "../reflection/description.private.h"
#include "../resources/universe.private.h"
#include "compound.private.h"
#include "route.h"
#include "traversal.private.h"

namespace ayu {
namespace in {

struct ScanContext;

struct ScanTraversalHead {
    ScanContext* context;
    RouteRef rt;
};

template <class T = Traversal>
struct ScanTraversal : ScanTraversalHead, T { };

struct ScanContext {
    CallbackRef<void(const ScanTraversal<>&)> cb;
    bool done = false;
};

struct TraverseScan {

    static
    bool start_pointers (
        AnyPtr base_item, RouteRef base_rt, ScanPointersCB cb
    ) {
        if (currently_scanning) {
            raise(e_ScanWhileScanning,
                "Cannot start scan while there's already a scan running."
            );
        }
        currently_scanning = true;
        ScanContext ctx {
            CallbackRef<void(const ScanTraversal<>&)>(
                cb, [](auto& cb, const ScanTraversal<>& trav) {
                    if (!(trav.caps & AC::AddressChildren)) return;
                    bool done = !!(trav.caps & AC::Address) &&
                        cb(AnyPtr(trav.type, trav.address, trav.caps), trav.rt);
                    if (done) [[unlikely]] trav.context->done = true;
                    else after_cb(trav);
                }
            )
        };
        CurrentBase curb (base_rt, base_item);
        ScanTraversal<StartTraversal> child;
        child.context = &ctx;
        child.rt = base_rt;
        child.collapse_optional = false;
        trav_start<visit>(child, base_item, base_rt, AC::Read);
        currently_scanning = false;
        return ctx.done;
    }

    static
    bool start_references (
        const AnyRef& base_item, RouteRef base_rt, ScanReferencesCB cb,
        bool ignore_no_refs_to_children
    ) {
        using CBCB = void(decltype(cb)&, const ScanTraversal<>&);
        CBCB* cbcb = [](auto& cb, const ScanTraversal<>& trav) {
            bool done; {
                AnyRef ref;
                trav.to_reference(&ref);
                done = cb(ref, trav.rt);
            }
            if (done) [[unlikely]] trav.context->done = true;
            else after_cb(trav);
        };
        CBCB* cbcb_ignore = [](auto& cb, const ScanTraversal<>& trav) {
            bool done; {
                AnyRef ref;
                trav.to_reference(&ref);
                done = cb(ref, trav.rt);
            }
            if (done) [[unlikely]] trav.context->done = true;
            else after_cb_ignoring_no_refs_to_children(trav);
        };
        ScanContext ctx {
            CallbackRef<void(const ScanTraversal<>&)>(
                cb, ignore_no_refs_to_children ? cbcb_ignore : cbcb
            )
        };
        CurrentBase curb (base_rt, base_item);
        ScanTraversal<StartTraversal> child;
        child.context = &ctx;
        child.rt = base_rt;
        child.collapse_optional = false;
        trav_start<visit>(child, base_item, base_rt, AC::Read);
        currently_scanning = false;
        return ctx.done;
    }

    NOINLINE static
    void visit (const Traversal& tr) {
        auto& trav = static_cast<const ScanTraversal<>&>(tr);
        trav.context->cb(trav);
    }

     // TODO: use more parameters in use_*
    NOINLINE static
    void after_cb (const ScanTraversal<>& trav) {
        auto desc = trav.desc();
        if (!!(desc->type_flags & TypeFlags::NoRefsToChildren)) {
            return;
        }
        if (desc->preference() == DescFlags::PreferObject) {
            if (auto keys = desc->keys_acr()) {
                use_computed_attrs(trav, keys);
            }
            else use_attrs(trav, expect(desc->attrs()));
        }
        else if (desc->preference() == DescFlags::PreferArray) {
            if (auto length = desc->length_acr()) {
                if (!!(desc->flags & DescFlags::ElemsContiguous)) {
                    use_contiguous_elems(trav, length);
                }
                else use_computed_elems(trav, length);
            }
            else use_elems(trav, expect(desc->elems()));
        }
        else if (auto delegate = desc->delegate_acr()) {
            return use_delegate(trav, delegate);
        }
    }

    NOINLINE static
    void after_cb_ignoring_no_refs_to_children (const ScanTraversal<>& trav) {
        auto desc = trav.desc();
        if (desc->preference() == DescFlags::PreferObject) {
            if (auto keys = desc->keys_acr()) {
                use_computed_attrs(trav, keys);
            }
            else use_attrs(trav, expect(desc->attrs()));
        }
        else if (desc->preference() == DescFlags::PreferArray) {
            if (auto length = desc->length_acr()) {
                if (!!(desc->flags & DescFlags::ElemsContiguous)) {
                    use_contiguous_elems(trav, length);
                }
                else use_computed_elems(trav, length);
            }
            else use_elems(trav, expect(desc->elems()));
        }
        else if (auto delegate = desc->delegate_acr()) {
            return use_delegate(trav, delegate);
        }
    }

    NOINLINE static
    void use_attrs (const ScanTraversal<>& trav, const AttrsDcrPrivate* attrs) {
        for (u32 i = 0; i < attrs->n_attrs; i++) {
            auto attr = attrs->attr(i);
             // Not discarding invisible attrs for scan purposes.
            auto acr = attr->acr();
            SharedRoute child_rt;
             // TODO: verify that the child item is object-like.
            ScanTraversal<AttrTraversal> child;
            child.context = trav.context;
            if (!!(acr->attr_flags & AttrFlags::Include)) {
                 // Behave as though all included attrs are included (collapse
                 // the route segment for the included attr).
                child.rt = trav.rt;
            }
            else {
                child_rt = SharedRoute(trav.rt, attr->key);
                child.rt = child_rt;
            }
            child.collapse_optional =
                !!(acr->attr_flags & AttrFlags::CollapseOptional);
            trav_attr<visit>(child, trav, acr, attr->key, AC::Read);
            child_rt = {};
            if (child.context->done) [[unlikely]] return;
        }
    }

    NOINLINE static
    void use_computed_attrs (
        const ScanTraversal<>& trav, const Accessor* keys_acr
    ) {
         // Get list of keys
        AnyArray<AnyString> keys;
        keys_acr->read(*trav.address,
            AccessCB(keys, [](auto& keys, Type t, Mu* v)
        {
            auto& ks = require_readable_keys(t, v);
            new (&keys) AnyArray<AnyString>(ks);
        }));
        auto f = expect(trav.desc()->computed_attrs())->f;
         // Now scan for each key
        for (auto& key : keys) {
            auto ref = f(*trav.address, key);
            if (!ref) raise_AttrNotFound(trav.type, key);
            auto child_rt = SharedRoute(trav.rt, key);
            ScanTraversal<ComputedAttrTraversal> child;
            child.context = trav.context;
            child.rt = child_rt;
            child.collapse_optional = false;
            trav_computed_attr<visit>(
                child, trav, ref, f, key, AC::Read
            );
            if (child.context->done) [[unlikely]] return;
        }
    }

    NOINLINE static
    void use_elems (const ScanTraversal<>& trav, const ElemsDcrPrivate* elems) {
        for (u32 i = 0; i < elems->n_elems; i++) {
            auto elem = elems->elem(i);
            auto acr = elem->acr();
            SharedRoute child_rt;
            ScanTraversal<ElemTraversal> child;
            child.context = trav.context;
            if (trav.collapse_optional) [[unlikely]] {
                 // It'd be weird to specify collapse_optional when the child
                 // item uses non-computed elems, but it's valid.  TODO: assert
                 // that there aren't multiple elems?
                child.rt = trav.rt;
            }
            else {
                child_rt = SharedRoute(trav.rt, i);
                child.rt = child_rt;
            }
            child.collapse_optional = false;
             // TODO: verify that the child item is array-like.
            trav_elem<visit>(child, trav, acr, i, AC::Read);
            if (child.context->done) [[unlikely]] return;
        }
    }

    NOINLINE static
    void use_computed_elems (
        const ScanTraversal<>& trav, const Accessor* length_acr
    ) {
        u32 len;
        read_length_acr(len, trav.type, trav.address, length_acr);
        auto f = expect(trav.desc()->computed_elems())->f;
        for (u32 i = 0; i < len; i++) {
            auto ref = f(*trav.address, i);
            if (!ref) raise_ElemNotFound(trav.type, i);
            SharedRoute child_rt;
            ScanTraversal<ComputedElemTraversal> child;
            child.context = trav.context;
            if (trav.collapse_optional) {
                child.rt = trav.rt;
            }
            else {
                child_rt = SharedRoute(trav.rt, i);
                child.rt = child_rt;
            }
            child.collapse_optional = false;
            trav_computed_elem<visit>(
                child, trav, ref, f, i, AC::Read
            );
            child_rt = {};
            if (child.context->done) [[unlikely]] return;
        }
    }

    NOINLINE static
    void use_contiguous_elems (
        const ScanTraversal<>& trav, const Accessor* length_acr
    ) {
        u32 len;
        read_length_acr(len, trav.type, trav.address, length_acr);
        if (!len) return;
        auto f = expect(trav.desc()->contiguous_elems())->f;
        auto ptr = f(*trav.address);
        for (u32 i = 0; i < len; i++) {
            SharedRoute child_rt;
            ScanTraversal<ContiguousElemTraversal> child;
            child.context = trav.context;
            child.collapse_optional = false;
            if (trav.collapse_optional) {
                child.rt = trav.rt;
            }
            else {
                child_rt = SharedRoute(trav.rt, i);
                child.rt = child_rt;
            }
            trav_contiguous_elem<visit>(
                child, trav, ptr, f, i, AC::Read
            );
            child_rt = {};
            if (child.context->done) [[unlikely]] return;
            ptr.address = (Mu*)((char*)ptr.address + ptr.type().cpp_size());
        }
    }

    NOINLINE static
    void use_delegate (const ScanTraversal<>& trav, const Accessor* acr) {
        ScanTraversal<DelegateTraversal> child;
        child.context = trav.context;
        child.rt = trav.rt;
        child.collapse_optional = trav.collapse_optional;
        trav_delegate<visit>(child, trav, acr, AC::Read);
    }
};

 // Store a typed AnyPtr instead of a Mu* because items at the same address
 // with different types are different items.
static UniqueArray<Pair<AnyPtr, SharedRoute>> route_cache;
static bool have_route_cache = false;
static u32 keep_route_cache_count = 0;

NOINLINE // Noinline the slow path to make the callback leaner
bool realloc_cache (auto& cache, AnyPtr ptr, RouteRef rt) {
    cache.reserve_plenty(cache.size() + 1);
    expect(rt);
    cache.emplace_back_expect_capacity(ptr, rt);
    return false;
}

bool get_route_cache () {
    if (!keep_route_cache_count) return false;
    if (!have_route_cache) {
        plog("Generate route cache begin");
        route_cache.reserve(256);
        scan_universe_pointers(CallbackRef<bool(AnyPtr, RouteRef)>(
            route_cache, [](auto& cache, AnyPtr ptr, RouteRef rt)
        {
             // We're deliberately ignoring the case where the same typed
             // pointer turns up twice in the data tree.  If this happens, we're
             // probably dealing with some sort of shared_ptr-like situation,
             // and in that case it shouldn't matter which route gets cached.
             // It could theoretically be a problem if the pointers differ in
             // readonlyness, but that should probably never happen.
            expect(cache.owned());
            if (cache.size() < cache.capacity()) {
                expect(rt);
                cache.emplace_back_expect_capacity(ptr, rt);
                return false;
            }
            else return realloc_cache(cache, ptr, rt);
        }));
        plog("Generate route cache sort");
         // Disable refcounting while sorting
        auto uncounted = route_cache.reinterpret<Pair<AnyPtr, RouteRef>>();
        std::sort(uncounted.begin(), uncounted.end(),
            [](const auto& a, const auto& b){ return a.first < b.first; }
        );
        have_route_cache = true;
        plog("Generate route cache end");
#ifdef AYU_PROFILE
        fprintf(stderr, "Route cache entries: %ld\n", route_cache.size());
#endif
    }
    return true;
}

 // This optimization interferes with conditional move conversion in recent gcc
[[gnu::optimize("-fno-thread-jumps")]]
const Pair<AnyPtr, SharedRoute>* search_route_cache (AnyPtr item) {
    if (!have_route_cache) return null;
    u32 bottom = 0;
    u32 top = route_cache.size();
    while (top != bottom) {
        u32 mid = (top + bottom) / 2;
        auto& e = route_cache[mid];
        if (e.first.address == item.address) {
            Type aa = e.first.type();
            Type bb = item.type();
            if (aa == bb) return &e;
            bool up = aa < bb;
            if (up) bottom = mid + 1;
            if (!up) top = mid;
        }
        else {
            bool up = e.first.address < item.address;
            if (up) bottom = mid + 1;
            if (!up) top = mid;
        }
    }
    return null;
}

} using namespace in;

KeepRouteCache::KeepRouteCache () noexcept {
    keep_route_cache_count++;
}
KeepRouteCache::~KeepRouteCache () {
    if (!--keep_route_cache_count) {
        have_route_cache = false;
        route_cache.clear();
    }
}

bool scan_pointers (
    AnyPtr base_item, RouteRef base_rt, ScanPointersCB cb
) {
    return TraverseScan::start_pointers(base_item, base_rt, cb);
}

bool scan_references (
    const AnyRef& base_item, RouteRef base_rt, ScanReferencesCB cb
) {
    return TraverseScan::start_references(base_item, base_rt, cb, false);
}

bool scan_references_ignoring_no_refs_to_children (
    const AnyRef& base_item, RouteRef base_rt, ScanReferencesCB cb
) {
    return TraverseScan::start_references(base_item, base_rt, cb, true);
}

bool scan_resource_pointers (ResourceRef res, ScanPointersCB cb) {
    auto& value = res->get_value();
    if (!value) return false;
    return scan_pointers(value.ptr(), SharedRoute(res), cb);
}

bool scan_resource_references (ResourceRef res, ScanReferencesCB cb) {
    auto& value = res->get_value();
    if (!value) return false;
    return scan_references(value.ptr(), SharedRoute(res), cb);
}

bool scan_universe_pointers (ScanPointersCB cb) {
    if (current_base) {
        auto rt = current_base->route;
        if (auto ref = rt->reference())
        if (auto address = ref->address()) {
           scan_pointers(address, rt, cb);
        }
    }
    for (auto& [_, res] : universe().resources) {
        if (scan_resource_pointers(res, cb)) return true;
    }
    return false;
}

bool scan_universe_references (ScanReferencesCB cb) {
     // To allow serializing self-referential data structures that aren't inside
     // a Resource, first scan the currently-being-serialized item, but only if
     // it's not in a Resource (so we don't duplicate work).
     // TODO: Maybe don't do this if the traversal was started by a scan,
     // instead of by a serialize.
    if (current_base) {
        auto rt = current_base->route;
        if (auto ref = rt->reference())
        if (scan_references(*ref, rt, cb)) {
            return true;
        }
    }
    for (auto& [_, res] : universe().resources) {
        if (scan_resource_references(res, cb)) return true;
    }
    return false;
}

SharedRoute find_pointer (AnyPtr item) {
    if (!item) return {};
    for (auto plr = first_plr; plr; plr = plr->next) {
        if (AnyRef(item) == plr->reference) return plr->route;
    }
    if (get_route_cache()) {
        if (auto it = search_route_cache(item)) {
             // Reject non-readonly pointer to readonly route
            if (it->first.readonly() && !item.readonly()) {
                [[unlikely]] return {};
            }
            return it->second;
        }
        return {};
    }
    else {
        SharedRoute r;
        scan_universe_pointers([&r, item](AnyPtr p, RouteRef rt){
            if (p == item) {
                 // If we get a non-readonly pointer to a readonly route,
                 // reject it, but also don't keep searching.
                if (p.readonly() && !item.readonly()) [[unlikely]] return true;
                new (&r) SharedRoute(rt);
                return true;
            }
            return false;
        });
        return r;
    }
}

SharedRoute find_reference (const AnyRef& item) {
    if (!item) return {};
    for (auto plr = first_plr; plr; plr = plr->next) {
        if (item == plr->reference) return plr->route;
    }
    if (get_route_cache()) {
        if (AnyPtr ptr = item.address()) {
             // Addressable! This will be fast.
            if (auto it = search_route_cache(ptr)) {
                if (item.caps() > it->first.caps()) {
                    [[unlikely]] return {};
                }
                return it->second;
            }
            return {};
        }
        else {
             // Not addressable.  First find the host in the route cache.
            if (auto it = search_route_cache(item.host)) {
                 // Now search under that host for the actual reference.
                SharedRoute r;
                scan_references(
                    AnyRef(item.host), it->second,
                    [&r, &item](const AnyRef& ref, RouteRef rt)
                {
                    if (ref == item) {
                        if (item.caps() > ref.caps()) {
                            [[unlikely]] return true;
                        }
                        new (&r) SharedRoute(rt);
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
         // We don't have the route cache!  Time to do a global search.
        SharedRoute r;
        scan_universe_references(
            [&r, &item](const AnyRef& ref, RouteRef rt)
        {
            if (ref == item) {
                if (item.caps() > ref.caps()) {
                    [[unlikely]] return true;
                }
                new (&r) SharedRoute(rt);
                return true;
            }
            else return false;
        });
        return r;
    }
}

SharedRoute pointer_to_route (AnyPtr item) {
    if (!item) return {};
    else if (SharedRoute r = find_pointer(item)) {
        return r;
    }
    else raise(e_ReferenceNotFound, cat(
        "Couldn't locate pointer target of type ", item.type().name()
    ));
}

SharedRoute reference_to_route (const AnyRef& item) {
    if (!item) return {};
    else if (SharedRoute r = find_reference(item)) {
        return r;
    }
    else raise(e_ReferenceNotFound, cat(
        "Couldn't locate reference target of type ", item.type().name()
    ));
}

bool currently_scanning = false;

} using namespace ayu;
