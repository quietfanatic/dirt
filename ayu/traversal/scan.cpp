#include "scan.h"
#include <algorithm>
#include "../reflection/anyptr.h"
#include "../reflection/link.h"
#include "../reflection/description.private.h"
#include "../resources/resource.private.h"
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
            raise(e_ForbiddenWhileScanning, "Cannot start scan while there's already a scan running.");
        }
        currently_scanning = true;
        ScanContext ctx {
            CallbackRef<void(const ScanTraversal<>&)>(
                cb, [](auto& cb, const ScanTraversal<>& trav) {
                    if (!(trav.caps % AC::AddressChildren)) return;
                    bool done = trav.caps % AC::Address &&
                        cb(AnyPtr(trav.type, trav.address, trav.caps), trav.rt);
                    if (done) [[unlikely]] trav.context->done = true;
                    else after_cb(trav);
                }
            )
        };
         // Don't set the current base when scanning.  The scanning process was
         // likely started implicitly and shouldn't need it.
        ScanTraversal<StartTraversal> child;
        child.context = &ctx;
        child.rt = base_rt;
        child.collapse_optional = false;
        trav_start<visit>(child, base_item, AC::Read);
        currently_scanning = false;
        return ctx.done;
    }

    static
    bool start_links (
        const Link& base_item, RouteRef base_rt, ScanLinksCB cb,
        bool ignore_no_links_to_children
    ) {
        using CBCB = void(decltype(cb)&, const ScanTraversal<>&);
        CBCB* cbcb = [](auto& cb, const ScanTraversal<>& trav) {
            bool done; {
                Link l;
                trav.to_link(&l);
                done = cb(l, trav.rt);
            }
            if (done) [[unlikely]] trav.context->done = true;
            else after_cb(trav);
        };
        CBCB* cbcb_ignore = [](auto& cb, const ScanTraversal<>& trav) {
            bool done; {
                Link l;
                trav.to_link(&l);
                done = cb(l, trav.rt);
            }
            if (done) [[unlikely]] trav.context->done = true;
            else after_cb_ignoring_no_links_to_children(trav);
        };
        ScanContext ctx {
            CallbackRef<void(const ScanTraversal<>&)>(
                cb, ignore_no_links_to_children ? cbcb_ignore : cbcb
            )
        };
        ScanTraversal<StartTraversal> child;
        child.context = &ctx;
        child.rt = base_rt;
        child.collapse_optional = false;
        child.collapsed_elem_shift = 0;
        trav_start<visit>(child, base_item, AC::Read);
        currently_scanning = false;
        return ctx.done;
    }

    NOINLINE static
    void visit (const Traversal& tr) {
        auto& trav = static_cast<const ScanTraversal<>&>(tr);
         // TODO TODO TODO make this not possible somehow
        if (!trav.address) [[unlikely]] return;
        trav.context->cb(trav);
    }

    NOINLINE static
    void after_cb (const ScanTraversal<>& trav) {
        auto desc = trav.desc();
        if (desc->type_flags % TypeFlags::NoLinksToChildren) {
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
                if (desc->flags % DescFlags::ElemsContiguous) {
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
    void after_cb_ignoring_no_links_to_children (const ScanTraversal<>& trav) {
        auto desc = trav.desc();
        if (desc->preference() == DescFlags::PreferObject) {
            if (auto keys = desc->keys_acr()) {
                use_computed_attrs(trav, keys);
            }
            else use_attrs(trav, expect(desc->attrs()));
        }
        else if (desc->preference() == DescFlags::PreferArray) {
            if (auto length = desc->length_acr()) {
                if (desc->flags % DescFlags::ElemsContiguous) {
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
             // Scan invisible attrs as well
            auto acr = attr->acr();
            SharedRoute child_rt;
             // TODO: verify that the child item is object-like.
            ScanTraversal<AttrTraversal> child;
            child.context = trav.context;
            if (acr->attr_flags % AttrFlags::Collapse) {
                 // Behave as though all collapsed attrs are collapsed (leave
                 // out the route segment for the collapsed attr).
                child.rt = trav.rt;
            }
            else {
                child_rt = SharedRoute(trav.rt, attr->key);
                child.rt = child_rt;
            }
            child.collapse_optional = acr->attr_flags % AttrFlags::CollapseOptional;
            child.collapsed_elem_shift = 0;
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
        SharedArray<SharedString> keys;
        keys_acr->read(*trav.address,
            AccessCB(keys, [](auto& keys, Type t, Mu* v)
        {
            auto& ks = require_readable_keys(t, v);
            new (&keys) SharedArray<SharedString>(ks);
        }));
        auto f = expect(trav.desc()->computed_attrs())->f;
         // Now scan for each key
        for (auto& key : keys) {
            auto link = f(*trav.address, key);
            if (!link) raise_AttrNotFound(trav.type, key);
            auto child_rt = SharedRoute(trav.rt, key);
            ScanTraversal<ComputedAttrTraversal> child;
            child.context = trav.context;
            child.rt = child_rt;
            child.collapse_optional = false;
            child.collapsed_elem_shift = 0;
            trav_computed_attr<visit>(
                child, trav, link, f, key, AC::Read
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
            child.collapse_optional = false;
            child.collapsed_elem_shift = 0;
            if (acr->attr_flags % AttrFlags::Collapse) {
                if (trav.collapse_optional) [[unlikely]] {
                     // Not sure how this interacts with collapse_optional on
                     // parent, but I think we can ignore it?
                    if (i >= 1) {
                        raise(e_General, "collapse_optional on array bigger than 1");
                    }
                }
                child.rt = trav.rt;
                child.collapsed_elem_shift = i + trav.collapsed_elem_shift;
            }
            else if (trav.collapse_optional) [[unlikely]] {
                 // It'd be weird to specify collapse_optional when the child
                 // item uses non-computed elems, but it's valid.
                if (i >= 1) {
                    raise(e_General, "collapse_optional on array bigger than 1");
                }
                child.rt = trav.rt;
            }
            else {
                child_rt = SharedRoute(trav.rt, i + trav.collapsed_elem_shift);
                child.rt = child_rt;
            }
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
            auto link = f(*trav.address, i);
            if (!link) raise_ElemNotFound(trav.type, i);
            SharedRoute child_rt;
            ScanTraversal<ComputedElemTraversal> child;
            child.context = trav.context;
            child.collapse_optional = false;
            child.collapsed_elem_shift = 0;
            if (trav.collapse_optional) {
                if (i >= 1) {
                    raise(e_General, "collapse_optional on array bigger than 1");
                }
                child.rt = trav.rt;
            }
            else {
                child_rt = SharedRoute(trav.rt, i + trav.collapsed_elem_shift);
                child.rt = child_rt;
            }
            trav_computed_elem<visit>(
                child, trav, link, f, i, AC::Read
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
            child.collapsed_elem_shift = 0;
            if (trav.collapse_optional) {
                if (i >= 1) {
                    raise(e_General, "collapse_optional on array bigger than 1");
                }
                child.rt = trav.rt;
            }
            else {
                child_rt = SharedRoute(trav.rt, i + trav.collapsed_elem_shift);
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
        child.collapsed_elem_shift = trav.collapsed_elem_shift;
        trav_delegate<visit>(child, trav, acr, AC::Read);
    }
};

 // Store a typed AnyPtr instead of a Mu* because items at the same address
 // with different types are different items.
static UniqueArray<Pair<AnyPtr, SharedRoute>> route_cache;
static bool have_route_cache = false;

NOINLINE // Noinline the slow path to make the callback leaner
bool realloc_cache (auto& cache, AnyPtr ptr, RouteRef rt) {
    cache.reserve_plenty(cache.size() + 1);
    expect(rt);
    cache.emplace_back_expect_capacity(ptr, rt);
    return false;
}

NOINLINE
void gen_route_cache () {
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

bool get_route_cache () {
    if (!keep_route_cache_count) return false;
    if (!have_route_cache) gen_route_cache();
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

void clear_route_cache () {
    have_route_cache = false;
    route_cache.clear();
}

} using namespace in;

NOINLINE
bool scan_pointers (
    AnyPtr base_item, RouteRef base_rt, ScanPointersCB cb
) {
    return TraverseScan::start_pointers(base_item, base_rt, cb);
}

NOINLINE
bool scan_links (
    const Link& base_item, RouteRef base_rt, ScanLinksCB cb
) {
    return TraverseScan::start_links(base_item, base_rt, cb, false);
}

bool scan_links_ignoring_no_links_to_children (
    const Link& base_item, RouteRef base_rt, ScanLinksCB cb
) {
    return TraverseScan::start_links(base_item, base_rt, cb, true);
}

bool scan_resource_pointers (ResourceRef res, ScanPointersCB cb) {
    auto& value = res->get_value();
    if (!value) return false;
    return scan_pointers(value.ptr(), SharedRoute(res), cb);
}

bool scan_resource_links (ResourceRef res, ScanLinksCB cb) {
     // TODO: use FakeRef or whatever replaces it
    auto& value = res->get_value();
    if (!value) return false;
    return scan_links(value.ptr(), SharedRoute(res), cb);
}

bool scan_universe_pointers (ScanPointersCB cb) {
    if (current_base) {
        if (auto link = current_base->link())
        if (auto address = link->address()) {
           scan_pointers(address, current_base, cb);
        }
    }
    for (auto [h, res] : g_universe->resources) {
        if (scan_resource_pointers(res, cb)) return true;
    }
    return false;
}

bool scan_universe_links (ScanLinksCB cb) {
     // To allow serializing self-referential data structures that aren't inside
     // a Resource, first scan the currently-being-serialized item, but only if
     // it's not in a Resource (so we don't duplicate work).
    if (current_base) {
        if (auto link = current_base->link())
        if (scan_links(*link, current_base, cb)) {
            return true;
        }
    }
    for (auto [h, res] : g_universe->resources) {
        if (scan_resource_links(res, cb)) return true;
    }
    return false;
}

SharedRoute find_pointer (AnyPtr item) {
    if (!item) return {};
    for (auto pll = first_pll; pll; pll = pll->next) {
        if (Link(item) == pll->link) return pll->route;
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

SharedRoute find_link (const Link& item) {
    if (!item) return {};
    for (auto pll= first_pll; pll; pll = pll->next) {
        if (item == pll->link) return pll->route;
    }
     // Since Link no longer stores its host type, when given an unaddressable
     // Link we can no longer find the host in the route cache, then scan
     // under the host for the referent.  So if we have an unaddressable Link
     // we just gotta search the entire universe now, though this should happen
     // rarely if ever.  If this does become a problem, we could cache Links
     // instead of AnyPtrs, but then we'd have to define operator<=> on Link.
    if (item.addressable() && get_route_cache()) {
        AnyPtr ptr = item.address();
        if (auto it = search_route_cache(ptr)) {
            if (!contains(it->first.caps(), item.caps())) {
                [[unlikely]] return {};
            }
            return it->second;
        }
        return {};
    }
    else {
         // Gotta do a global search
        SharedRoute r;
        scan_universe_links(
            [&r, &item](const Link& link, RouteRef rt)
        {
            if (link == item) {
                if (!contains(link.caps(), item.caps())) {
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
    else raise(e_LinkNotFound, cat(
        "Couldn't locate pointer target of type ", item.type().name()
    ));
}

SharedRoute link_to_route (const Link& item) {
    if (!item) return {};
    else if (SharedRoute r = find_link(item)) {
        return r;
    }
    else raise(e_LinkNotFound, cat(
        "Couldn't locate link target of type ", item.type().name()
    ));
}

 // Don't leave yet, wait for me!
bool currently_scanning = false;

} using namespace ayu;
