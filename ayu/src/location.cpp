#include "../location.h"

#include <charconv>
#include "../../iri/iri.h"
#include "../describe.h"
#include "../errors.h"
#include "../serialize.h"
#include "traversal-private.h"

namespace ayu {
namespace in {

NOINLINE
void delete_LocationData (LocationData* p) noexcept {
    switch (p->form) {
        case RESOURCE: delete static_cast<ResourceLocation*>(p); break;
        case REFERENCE: delete static_cast<ReferenceLocation*>(p); break;
        case KEY: delete static_cast<KeyLocation*>(p); break;
        case INDEX: delete static_cast<IndexLocation*>(p); break;
        default: never();
    }
}

} using namespace in;

 // It would be nice to be able to use Traversal for this, but this walks
 // upwards and Traversal only walks downwards.
Reference reference_from_location (LocationRef loc) {
    if (!*loc) return Reference();
    switch (loc->data->form) {
        case RESOURCE: return loc->resource()->ref();
        case REFERENCE: return *loc->reference();
        case KEY: return item_attr(
            reference_from_location(*loc->parent()),
            *loc->key(), *loc->parent()
        );
        case INDEX: return item_elem(
            reference_from_location(*loc->parent()),
            *loc->index(), *loc->parent()
        );
        default: never();
    }
}

Location current_base_location () noexcept {
    if (auto trav = Traversal::current_start) {
        if (*trav->location) return trav->location->root();
        else return Location(*trav->reference);
    }
    else return Location();
}

IRI current_base_iri () noexcept {
    return location_to_iri(current_base_location());
}

namespace in {

static const IRI anonymous_iri ("ayu-anonymous:");

NOINLINE static
UniqueString location_to_iri_accumulate (const IRI*& base, LocationRef loc) {
    switch (loc->data->form) {
        case RESOURCE: base = &loc->resource()->name(); return "#";
        case REFERENCE: base = &anonymous_iri; return "#";
        case KEY: return cat(
            location_to_iri_accumulate(base, *loc->parent()),
            '/', *loc->key()
        );
        case INDEX: return cat(
            location_to_iri_accumulate(base, *loc->parent()),
            '+', *loc->index()
        );
        default: never();
    }
}

} // in

IRI location_to_iri (LocationRef loc) noexcept {
    if (!*loc) return IRI();
    const IRI* base;
    UniqueString fragment = location_to_iri_accumulate(base, loc);
    return IRI(fragment, *base);
}

Location location_from_iri (const IRI& iri) {
    if (iri.empty()) return Location();
    if (!iri) throw InvalidLocationIRI(
        iri.possibly_invalid_spec(), "iri is an invalid iri by itself"
    );
    if (!iri.has_fragment()) throw InvalidLocationIRI(
        iri.possibly_invalid_spec(), "iri does not have a #fragment"
    );
    auto root_iri = iri.without_fragment();
    Location r;
    if (root_iri == anonymous_iri) {
        r = current_base_location();
    }
    else r = Location(Resource(root_iri));
    Str fragment = iri.fragment();
    usize i = 0;
    while (i < fragment.size()) {
        if (fragment[i] == '/') {
            usize start = ++i;
            while (
                i < fragment.size() && fragment[i] != '/' && fragment[i] != '+'
            ) ++i;
            r = Location(move(r), iri::decode(fragment.slice(start, i)));
        }
        else if (fragment[i] == '+') {
            const char* start = fragment.begin() + ++i;
            usize index;
            auto [ptr, ec] = std::from_chars(
                start, fragment.end(), index
            );
            if (ptr == start) throw InvalidLocationIRI(
                iri.spec(), "invalid +index in #fragment"
            );
            i += ptr - start;
            r = Location(move(r), index);
        }
        else if (i == 0) throw InvalidLocationIRI(
            iri.spec(), "#fragment doesn't start with / or +"
        );
        else throw InvalidLocationIRI(
            iri.spec(), "invalid +index in #fragment"
        );
    }
    return r;
}

} using namespace ayu;

AYU_DESCRIBE(ayu::Location,
    to_tree([](const Location& v){
        if (!v) return Tree("");
        auto iri = location_to_iri(v);
        return Tree(iri.spec_relative_to(current_base_iri()));
    }),
    from_tree([](Location& v, const Tree& t){
        auto rel = Str(t);
        if (!rel) v = Location();
        auto iri = IRI(rel, current_base_iri());
        v = location_from_iri(iri);
    })
);

// TODO: more tests

#ifndef TAP_DISABLE_TESTS
#include "test-environment-private.h"

static tap::TestSet tests ("dirt/ayu/location", []{
    using namespace tap;

    test::TestEnvironment env;

    auto loc = location_from_iri(IRI("ayu-test:/#/bar+1/bu%2Fp/+33+0/3//%2B/"));
    const Location* l = &loc;
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
    is(*l->resource(), Resource("ayu-test:/"), "Resource root");
    ok(!l->parent());

    done_testing();
});
#endif
