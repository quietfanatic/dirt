#include "location.h"

#include <charconv>
#include "../../iri/iri.h"
#include "../reflection/describe.h"
#include "../resources/resource.h"
#include "compound.h"
#include "traversal.private.h"

namespace ayu {
namespace in {

struct ResourceLocation : LocationData {
    Resource resource;
    ResourceLocation (MoveRef<Resource> res) :
        LocationData(RESOURCE), resource(*move(res))
    { }
};

} using namespace in;

Location::Location (Resource res) noexcept :
    data(new ResourceLocation(move(res)))
{ }

const Resource* Location::resource () const noexcept {
    switch (data->form) {
        case RESOURCE: return &static_cast<ResourceLocation*>(data.p)->resource;
        default: return null;
    }
}

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

namespace in {

static constexpr IRI anonymous_iri = IRI::Static("ayu-anonymous:");

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
    if (!iri) raise(e_LocationIRIInvalid, cat(
        "IRI is an invalid IRI by itself: ", iri.possibly_invalid_spec()
    ));
    if (!iri.has_fragment()) raise(e_LocationIRIInvalid, cat(
        "IRI does not have a #fragment: ", iri.possibly_invalid_spec()
    ));
    auto root_iri = iri.without_fragment();
    Location r = root_iri == current_base_iri()
        ? current_base_location()
        : Location(Resource(root_iri));
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
            if (ptr == start) raise(e_LocationIRIInvalid, cat(
                iri.spec(), "invalid +index in #fragment"
            ));
            i += ptr - start;
            r = Location(move(r), index);
        }
        else if (i == 0) raise(e_LocationIRIInvalid, cat(
            iri.spec(), "#fragment doesn't start with / or +"
        ));
        else raise(e_LocationIRIInvalid, cat(
            iri.spec(), "invalid +index in #fragment"
        ));
    }
    return r;
}

static Location cur_base_location;
IRI cur_base_iri;

Location current_base_location () noexcept { return cur_base_location; }

IRI current_base_iri () noexcept {
    if (!cur_base_iri) {
        cur_base_iri = location_to_iri(cur_base_location).without_fragment();
    }
    return cur_base_iri;
}

PushBaseLocation::PushBaseLocation (const Location& loc) noexcept :
    old_base_location(move(cur_base_location))
{
    cur_base_location = loc.root();
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
                    " (", item_to_string(&*loc), ')'
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
                " (", item_to_string(&*loc), ')'
            );
        }
        e.has_travloc = true;
        e.external = std::current_exception();
        throw e;
    }
}

} using namespace ayu;

AYU_DESCRIBE(ayu::Location,
    to_tree([](const Location& v){
        if (!v) return Tree("");
        auto iri = location_to_iri(v);
        return Tree(iri.make_relative(current_base_iri()));
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
#include "../test/test-environment.private.h"

static tap::TestSet tests ("dirt/ayu/traversal/location", []{
    using namespace tap;

    test::TestEnvironment env;

    auto loc = location_from_iri(IRI::Static("ayu-test:/#/bar+1/bu%2Fp/+33+0/3//%2B/"));
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
