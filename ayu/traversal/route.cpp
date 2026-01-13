#include "route.h"

#include <charconv>
#include "../../iri/iri.h"
#include "../reflection/describe.h"
#include "compound.h"
#include "traversal.private.h"

namespace ayu {
namespace in {

 // If we want, we could make this tail call itself, but it's probably not
 // important enough.  Routes are likely to be deleted one segment at a time
 // anyway.
NOINLINE
void delete_Route (const Route* p) noexcept {
    switch (p->form) {
        case RF::Resource: delete static_cast<const ResourceRoute*>(p); break;
        case RF::Reference: delete static_cast<const ReferenceRoute*>(p); break;
        case RF::Key: delete static_cast<const KeyRoute*>(p); break;
        case RF::Index: delete static_cast<const IndexRoute*>(p); break;
        default: never();
    }
}

} using namespace in;

 // It would be nice to be able to use Traversal for this, but Routes are kind
 // of inside-out compared to traversals, so we'd have to reverse it first.
AnyRef reference_from_route (RouteRef rt) {
    if (!rt) return AnyRef();
    switch (rt->form) {
        case RF::Resource: return rt->resource()->ref();
        case RF::Reference: return *rt->reference();
        case RF::Key: {
            auto self = static_cast<const in::KeyRoute*>(rt.data);
            AnyRef parent_ref = reference_from_route(self->parent);
            return item_attr(parent_ref, self->key, self->parent);
        }
        case RF::Index: {
            auto self = static_cast<const in::IndexRoute*>(rt.data);
            AnyRef parent_ref = reference_from_route(self->parent);
            return item_elem(parent_ref, self->index, self->parent);
        }
        default: never();
    }
}

namespace in {

struct RouteToIRI {
    UniqueString fragment;
    const IRI* base;

    NOINLINE
    char* use_base (RouteRef rt, u32 cap) {
        switch (rt->form) {
            case RF::Resource: base = &rt->resource()->name(); break;
            case RF::Reference: {
                 // Scuffed comparison with address in lieu of an actual route
                 // comparison function.
                if (current_base && &*current_base->route == &*rt) {
                    base = &anonymous_iri;
                    break;
                }
                else raise(e_RouteUnresolvable, "Cannot call route_to_iri on an anonymous reference-root-route unless it's the current anonymous item.");
            }
            default: never();
        }
        expect(cap > 0);
        new (&fragment) UniqueString (Uninitialized(cap));
        char* p = fragment.begin();
        *p++ = '#';
        return p;
    }

    NOINLINE
    char* use_key (RouteRef rt, u32 cap) {
        expect(rt->form == RF::Key);
        auto encoded = iri::encode(*rt->key());
        char* p = visit(rt->parent(), cap + 1 + encoded.size());
        *p++ = '/';
        expect(rt->form == RF::Key);
        char* r = p + encoded.size();
        std::memcpy(p, encoded.data(), encoded.size());
        return r;
    }

    NOINLINE
    char* use_small_index (RouteRef rt, u32 cap) {
        expect(rt->form == RF::Index);
        char* p = visit(rt->parent(), cap + 2);
        *p++ = '+';
        expect(rt->form == RF::Index);
        expect(*rt->index() < 10);
        *p++ = '0' + *rt->index();
        return p;
    }

    NOINLINE
    char* use_large_index (RouteRef rt, u32 cap) {
        expect(rt->form == RF::Index);
        expect(*rt->index() >= 10);
        u32 digits = count_decimal_digits(*rt->index());
        expect(rt->form == RF::Index);
        char* p = visit(rt->parent(), cap + 1 + digits);
        *p++ = '+';
        expect(rt->form == RF::Index);
        return write_decimal_digits(p, digits, *rt->index());
    }

    NOINLINE
    char* visit (RouteRef rt, u32 cap) {
        switch (rt->form) {
            case RF::Resource:
            case RF::Reference: return use_base(rt, cap);
            case RF::Key: return use_key(rt, cap);
            case RF::Index:
                if (*rt->index() < 10) {
                    return use_small_index(rt, cap);
                }
                else return use_large_index(rt, cap);
            default: never();
        }
    }
};
} // in

IRI route_to_iri (RouteRef rt) {
    if (!rt) return IRI();
    RouteToIRI rti;
    char* p = rti.visit(rt, 1);
    expect(p == rti.fragment.end());
    return IRI(rti.fragment, *rti.base);
}

SharedRoute route_from_iri (const IRI& iri) {
    if (iri.empty()) return {};
    if (!iri) raise(e_RouteIRIInvalid, cat(
        "Invalid IRI: ", iri.possibly_invalid_spec()
    ));
    if (!iri.has_fragment()) raise(e_RouteIRIInvalid, cat(
        "Route IRI does not have a #fragment: ", iri.spec()
    ));
    auto root_iri = iri.chop_fragment();
    Str spec = iri.spec();
    Str fragment = iri.fragment();
    auto p = fragment.begin();
    auto end = fragment.end();
    SharedRoute r;
    if (root_iri == anonymous_iri) {
        if (current_base && current_base->route->reference()) {
             // Allow addressing an item that isn't necessarily in a resource
            new (&r) SharedRoute(current_base->route);
        }
        else raise (e_RouteUnresolvable, cat(
            "Cannot call route_from_iri with ", anonymous_iri.spec(), " unless there is a current anonymous item."
        ));
    }
    else {
        new (&r) SharedRoute(ResourceRef(root_iri));
    }
    if (p < end && *p != '/' && *p != '+') {
         // #foo is a shortcut for #/foo+1
        auto start = p;
        while (
            p < end && *p != '/' && *p != '+'
        ) ++p;
        new (&r) SharedRoute(move(r), iri::decode(Str(start, p)));
        new (&r) SharedRoute(move(r), 1);
    }
    while (p < end) {
        if (*p == '/') {
            auto start = ++p;
            while (
                p < end && *p != '/' && *p != '+'
            ) ++p;
            new (&r) SharedRoute(move(r), iri::decode(Str(start, p)));
        }
        else if (*p == '+') {
            auto start = ++p;
            usize index;
            auto [ptr, ec] = std::from_chars(
                start, fragment.end(), index
            );
            if (ptr == start) goto invalid_index;
            p = ptr;
            new (&r) SharedRoute(move(r), index);
        }
        else goto invalid_index; // There's junk after a number.
    }
    return r;
    invalid_index:
    raise(e_RouteIRIInvalid, cat(
        "Invalid +index in #fragment: ", spec
    ));
}

static Tree route_to_tree (const RouteRef& v) {
    auto iri = route_to_iri(v);
    auto rel = iri.relative_to(current_base->iri());
    return Tree(rel);
}

} using namespace ayu;

AYU_DESCRIBE(ayu::SharedRoute,
    to_tree([](const SharedRoute& v){ return route_to_tree(v); }),
    from_tree([](SharedRoute& v, const Tree& t){
        auto rel = Str(t);
        auto iri = IRI(rel, current_base->iri());
        v = route_from_iri(iri);
    })
);

AYU_DESCRIBE(ayu::RouteRef,
    to_tree(&route_to_tree)
);

// TODO: more tests

#ifndef TAP_DISABLE_TESTS
#include "../test/test-environment.private.h"

static tap::TestSet tests ("dirt/ayu/traversal/route", []{
    using namespace tap;

    test::TestEnvironment env;

    SharedRoute rt = route_from_iri(IRI("ayu-test:/#/bar+1/bu%2Fp/+33+0/3//%2B/"));
    RouteRef r = rt;
    is(*r->key(), "", "Empty key");
    r = r->parent();
    is(*r->key(), "+", "Key with +");
    r = r->parent();
    is(*r->key(), "", "Empty key");
    r = r->parent();
    is(*r->key(), "3", "Number-like key");
    r = r->parent();
    is(*r->index(), 0u, "Index 0");
    r = r->parent();
    is(*r->index(), 33u, "Index 33");
    r = r->parent();
    is(*r->key(), "", "Empty key");
    r = r->parent();
    is(*r->key(), "bu/p", "String key with /");
    r = r->parent();
    is(*r->index(), 1u, "Index 1");
    r = r->parent();
    is(*r->key(), "bar", "String key");
    r = r->parent();
    is(r->resource(), SharedResource(IRI("ayu-test:/")), "Resource root");
    ok(!r->parent(), "No parent at root");
    is(r.data, rt->root().data, "Route::root()");

    done_testing();
});
#endif
