#include "location.h"

#include <charconv>
#include "../../iri/iri.h"
#include "../reflection/describe.h"
#include "../resources/resource.h"
#include "compound.h"
#include "traversal.private.h"

namespace ayu {
namespace in {

struct ResourceLocation : Location {
    SharedResource resource;
    ResourceLocation (MoveRef<SharedResource> res) :
        Location(LF::Resource), resource(*move(res))
    { }
};

} using namespace in;

SharedLocation::SharedLocation (ResourceRef res) noexcept :
    data(new ResourceLocation(expect(res)))
{ }

ResourceRef Location::resource () const noexcept {
    switch (form) {
        case LF::Resource: return static_cast<const ResourceLocation*>(this)->resource;
        default: return {};
    }
}

namespace in {

NOINLINE
void delete_Location (const Location* p) noexcept {
    switch (p->form) {
        case LF::Resource: delete static_cast<const ResourceLocation*>(p); break;
        case LF::Reference: delete static_cast<const ReferenceLocation*>(p); break;
        case LF::Key: delete static_cast<const KeyLocation*>(p); break;
        case LF::Index: delete static_cast<const IndexLocation*>(p); break;
        default: never();
    }
}

} using namespace in;

 // It would be nice to be able to use Traversal for this, but this walks
 // upwards and Traversal only walks downwards.
AnyRef reference_from_location (LocationRef loc) {
    if (!loc) return AnyRef();
    switch (loc->form) {
        case LF::Resource: return loc->resource()->ref();
        case LF::Reference: return *loc->reference();
        case LF::Key: return item_attr(
            reference_from_location(loc->parent()),
            *loc->key(), loc->parent()
        );
        case LF::Index: return item_elem(
            reference_from_location(loc->parent()),
            *loc->index(), loc->parent()
        );
        default: never();
    }
}

namespace in {

static constexpr IRI anonymous_iri ("ayu-anonymous:");

struct LocationToIRI {
    UniqueString fragment;
    const IRI* base;

    NOINLINE
    char* use_base (LocationRef loc, u32 cap) {
        switch (loc->form) {
            case LF::Resource: base = &loc->resource()->name(); break;
            case LF::Reference: base = &anonymous_iri; break;
            default: never();
        }
        expect(cap > 0);
        new (&fragment) UniqueString (Uninitialized(cap));
        char* p = fragment.begin();
        *p++ = '#';
        return p;
    }

    NOINLINE
    char* use_key (LocationRef loc, u32 cap) {
        expect(loc->form == LF::Key);
        char* p = visit(loc->parent(), cap + 1 + loc->key()->size());
        *p++ = '/';
        expect(loc->form == LF::Key);
        char* r = p + loc->key()->size();
        std::memcpy(p, loc->key()->data(), loc->key()->size());
        return r;
    }

    NOINLINE
    char* use_small_index (LocationRef loc, u32 cap) {
        expect(loc->form == LF::Index);
        char* p = visit(loc->parent(), cap + 2);
        *p++ = '+';
        expect(loc->form == LF::Index);
        expect(*loc->index() < 10);
        *p++ = '0' + *loc->index();
        return p;
    }

    NOINLINE
    char* use_large_index (LocationRef loc, u32 cap) {
        expect(loc->form == LF::Index);
        expect(*loc->index() >= 10);
        u32 digits = count_decimal_digits(*loc->index());
        expect(loc->form == LF::Index);
        char* p = visit(loc->parent(), cap + 1 + digits);
        *p++ = '+';
        expect(loc->form == LF::Index);
        return write_decimal_digits(p, digits, *loc->index());
    }

    NOINLINE
    char* visit (LocationRef loc, u32 cap) {
        switch (loc->form) {
            case LF::Resource:
            case LF::Reference: return use_base(loc, cap);
            case LF::Key: return use_key(loc, cap);
            case LF::Index:
                if (*loc->index() < 10) {
                    return use_small_index(loc, cap);
                }
                else return use_large_index(loc, cap);
            default: never();
        }
    }
};
} // in

IRI location_to_iri (LocationRef loc) noexcept {
    if (!loc) return IRI();
    LocationToIRI lti;
    char* p = lti.visit(loc, 1);
    expect(p == lti.fragment.end());
    return IRI(lti.fragment, *lti.base);
}

SharedLocation location_from_iri (const IRI& iri) {
    if (iri.empty()) return {};
    if (!iri) raise(e_LocationIRIInvalid, cat(
        "Invalid IRI: ", iri.possibly_invalid_spec()
    ));
    if (!iri.has_fragment()) raise(e_LocationIRIInvalid, cat(
        "Location IRI does not have a #fragment: ", iri.spec()
    ));
     // We could require that the location has a fragment, but instead lets
     // consider the lack of fragment to be equivalent to an empty fragment.
    auto root_iri = iri.chop_fragment();
    auto r = root_iri == current_base_iri()
        ? SharedLocation(current_base_location())
        : SharedLocation(ResourceRef(root_iri));
    Str fragment = iri.fragment();
    usize i = 0;
    while (i < fragment.size()) {
        if (fragment[i] == '/') {
            usize start = ++i;
            while (
                i < fragment.size() && fragment[i] != '/' && fragment[i] != '+'
            ) ++i;
            r = SharedLocation(move(r), iri::decode(fragment.slice(start, i)));
        }
        else if (fragment[i] == '+') {
            const char* start = fragment.begin() + ++i;
            usize index;
            auto [ptr, ec] = std::from_chars(
                start, fragment.end(), index
            );
            if (ptr == start) raise(e_LocationIRIInvalid, cat(
                iri.spec(), " invalid +index in #fragment"
            ));
            i += ptr - start;
            r = SharedLocation(move(r), index);
        }
        else if (i == 0) {
             // #foo is a shortcut for #/foo+1
            usize start = i;
            while (
                i < fragment.size() && fragment[i] != '/' && fragment[i] != '+'
            ) ++i;
            r = SharedLocation(move(r), iri::decode(fragment.slice(start, i)));
            r = SharedLocation(move(r), 1);
        }
        else {
             // We can get here if there's junk after a number.
            raise(e_LocationIRIInvalid, cat(
                iri.spec(), " invalid +index in #fragment"
            ));
        }
    }
    return r;
}

static SharedLocation cur_base_location;
IRI cur_base_iri;

LocationRef current_base_location () noexcept { return cur_base_location; }

IRI current_base_iri () noexcept {
    if (!cur_base_iri) {
        cur_base_iri = location_to_iri(cur_base_location).chop_fragment();
    }
    return cur_base_iri;
}

PushBaseLocation::PushBaseLocation (LocationRef loc) noexcept :
    old_base_location(move(cur_base_location))
{
    cur_base_location = loc->root();
    cur_base_iri = IRI();
}
PushBaseLocation::~PushBaseLocation () {
    cur_base_location = move(old_base_location);
    cur_base_iri = IRI();
}

void rethrow_with_travloc (LocationRef loc) {
    try { throw; }
    catch (Error& e) {
        if (!e.has_travloc) {
            e.has_travloc = true;
            {
                DiagnosticSerialization ds;
                e.details = cat(move(e.details),
                    " (", item_to_string(&loc), ')'
                );
            }
        }
        throw e;
    }
    catch (std::exception& ex) {
        Error e;
        e.code = e_External;
        {
            DiagnosticSerialization ds;
            e.details = cat(
                get_demangled_name(typeid(ex)), ": ", ex.what(),
                " (", item_to_string(&loc), ')'
            );
        }
        e.has_travloc = true;
        e.external = std::current_exception();
        throw e;
    }
}

NOINLINE static Tree location_to_tree (LocationRef v) {
    auto iri = location_to_iri(v);
    auto rel = iri.relative_to(current_base_iri());
    return Tree(rel);
}

} using namespace ayu;

AYU_DESCRIBE(ayu::SharedLocation,
    to_tree([](const SharedLocation& v){ return location_to_tree(v); }),
    from_tree([](SharedLocation& v, const Tree& t){
        auto rel = Str(t);
        auto iri = IRI(rel, current_base_iri());
        v = location_from_iri(iri);
    })
);

AYU_DESCRIBE(ayu::LocationRef,
    to_tree([](const LocationRef& v){ return location_to_tree(v); })
);

// TODO: more tests

#ifndef TAP_DISABLE_TESTS
#include "../test/test-environment.private.h"

static tap::TestSet tests ("dirt/ayu/traversal/location", []{
    using namespace tap;

    test::TestEnvironment env;

    SharedLocation loc = location_from_iri(IRI("ayu-test:/#/bar+1/bu%2Fp/+33+0/3//%2B/"));
    LocationRef l = loc;
    is(*l->key(), "", "Empty key");
    l = l->parent();
    is(*l->key(), "+", "Key with +");
    l = l->parent();
    is(*l->key(), "", "Empty key");
    l = l->parent();
    is(*l->key(), "3", "Number-like key");
    l = l->parent();
    is(*l->index(), 0u, "Index 0");
    l = l->parent();
    is(*l->index(), 33u, "Index 33");
    l = l->parent();
    is(*l->key(), "", "Empty key");
    l = l->parent();
    is(*l->key(), "bu/p", "String key with /");
    l = l->parent();
    is(*l->index(), 1u, "Index 1");
    l = l->parent();
    is(*l->key(), "bar", "String key");
    l = l->parent();
    is(l->resource(), SharedResource(IRI("ayu-test:/")), "Resource root");
    ok(!l->parent());

    done_testing();
});
#endif
