#include "../location.h"

#include <charconv>
#include "../describe.h"
#include "../reference.h"
#include "../resource.h"
#include "char-cases-private.h"

namespace ayu {
namespace in {

enum LocationForm {
    RESOURCE,
    REFERENCE,
    KEY,
    INDEX,
};

struct LocationData : RefCounted {
    uint8 form;
    LocationData (uint8 f) : form(f) { }
};

struct ResourceLocation : LocationData {
    Resource resource;
    ResourceLocation (Resource res) :
        LocationData(RESOURCE), resource(res)
    { }
};
struct ReferenceLocation : LocationData {
    Reference reference;
    ReferenceLocation (Reference ref) :
        LocationData(REFERENCE), reference(move(ref))
    { }
};

struct KeyLocation : LocationData {
    Location parent;
    AnyString key;
    KeyLocation (Location p, AnyString k) :
        LocationData(KEY), parent(move(p)), key(move(k))
    { }
};
struct IndexLocation : LocationData {
    Location parent;
    usize index;
    IndexLocation (Location p, usize i) :
        LocationData(INDEX), parent(move(p)), index(i)
    { }
};

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

Location::Location (Resource res) :
    data(new ResourceLocation(move(res)))
{ }
Location::Location (Reference ref) :
    data(new ReferenceLocation(move(ref)))
{ }
Location::Location (Location p, AnyString k) :
    data(new KeyLocation(expect(move(p)), move(k)))
{ }
Location::Location (Location p, usize i) :
    data(new IndexLocation(expect(move(p)), i))
{ }

const Resource* Location::resource () const {
    switch (data->form) {
        case RESOURCE: return &static_cast<ResourceLocation*>(data.p)->resource;
        default: return null;
    }
}

const Reference* Location::reference () const {
    switch (data->form) {
        case REFERENCE: return &static_cast<ReferenceLocation*>(data.p)->reference;
        default: return null;
    }
}

const Location* Location::parent () const {
    switch (data->form) {
        case KEY: return &static_cast<KeyLocation*>(data.p)->parent;
        case INDEX: return &static_cast<IndexLocation*>(data.p)->parent;
        default: return null;
    }
}
const AnyString* Location::key () const {
    switch (data->form) {
        case KEY: return &static_cast<KeyLocation*>(data.p)->key;
        default: return null;
    }
}
const usize* Location::index () const {
    switch (data->form) {
        case INDEX: return &static_cast<IndexLocation*>(data.p)->index;
        default: return null;
    }
}

Location Location::root () const {
    const Location* l = this;
    while (l->parent()) l = l->parent();
    return *l;
}

 // TODO: Use traversals for this
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

AnyString location_iri_to_relative_iri (const IRI& iri) {
    auto base = location_to_iri(current_root_location());
    expect(base.fragment().empty());
    return iri.spec_relative_to(base);
}

IRI location_iri_from_relative_iri (Str rel) {
    if (!rel) return IRI();
    auto base = location_to_iri(current_root_location());
    expect(base.fragment().empty());
    return IRI(rel, base);
}

namespace in {
static const IRI ayu_current_root ("ayu-current-root:");
static UniqueString location_to_iri_accumulate (const IRI*& base, LocationRef loc) {
    switch (loc->data->form) {
        case RESOURCE: base = &loc->resource()->name(); return "#";
         // TODO: This might be incorrect in some cases.  Check it
         // against current_root_location() maybe?
        case REFERENCE: base = &ayu_current_root; return "#";
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
}

IRI location_to_iri (LocationRef loc) {
    if (!*loc) return IRI();
    const IRI* base;
    UniqueString fragment = location_to_iri_accumulate(base, loc);
    return IRI(fragment, *base);
}

Location location_from_iri (const IRI& iri) {
    if (!iri) {
        if (iri.is_empty()) return Location();
        else throw InvalidLocationIRI(
            iri.possibly_invalid_spec(), "iri is an invalid iri by itself"
        );
    }
    auto root_iri = iri.iri_without_fragment();
    Location r;
    if (root_iri == ayu_current_root) {
        r = current_root_location();
    }
    else r = Location(Resource(root_iri));
    Str fragment = iri.fragment();
    usize i = 0;
    while (i < fragment.size()) {
        if (fragment[i] == '/') {
            usize start = ++i;
            for (; i < fragment.size(); ++i) {
                if (fragment[i] == '/' || fragment[i] == '+') {
                    break;
                }
            }
            r = Location(move(r), iri::decode(fragment.slice(start, i)));
        }
        else if (fragment[i] == '+') {
            const char* start = fragment.begin() + ++i;
            usize index;
            auto [ptr, ec] = std::from_chars(
                start, fragment.end(), index
            );
            if (ptr == start) {
                throw InvalidLocationIRI(iri.spec(), "invalid +index in fragment");
            }
            i += ptr - start;
            r = Location(move(r), index);
        }
        else {
            if (i == 0) throw InvalidLocationIRI(
                iri.spec(), "fragment doesn't start with / or +"
            );
            else throw InvalidLocationIRI(
                iri.spec(), "invalid +index in fragment"
            );
        }
    }
    return r;
}

} using namespace ayu;

AYU_DESCRIBE(ayu::Location,
    to_tree([](const Location& v){
        auto iri = location_to_iri(v);
        return Tree(location_iri_to_relative_iri(iri));
    }),
    from_tree([](Location& v, const Tree& t){
        auto iri = location_iri_from_relative_iri(Str(t));
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
