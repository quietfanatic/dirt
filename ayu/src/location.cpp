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
    if (!data) return null;
    switch (data->form) {
        case RESOURCE: return &static_cast<ResourceLocation*>(data.p)->resource;
        default: return null;
    }
}

const Reference* Location::reference () const {
    if (!data) return null;
    switch (data->form) {
        case REFERENCE: return &static_cast<ReferenceLocation*>(data.p)->reference;
        default: return null;
    }
}

const Location* Location::parent () const {
    if (!data) return null;
    switch (data->form) {
        case KEY: return &static_cast<KeyLocation*>(data.p)->parent;
        case INDEX: return &static_cast<IndexLocation*>(data.p)->parent;
        default: return null;
    }
}
const AnyString* Location::key () const {
    if (!data) return null;
    switch (data->form) {
        case KEY: return &static_cast<KeyLocation*>(data.p)->key;
        default: return null;
    }
}
const usize* Location::index () const {
    if (!data) return null;
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

bool operator == (LocationRef a, LocationRef b) noexcept {
    if (a->data == b->data) return true;
    if (!a->data || !b->data) return false;
    if (a->data->form != b->data->form) return false;
    switch (a->data->form) {
        case RESOURCE: return *a->resource() == *b->resource();
        case REFERENCE: return *a->reference() == *b->reference();
        case KEY: return *a->key() == *b->key();
        case INDEX: return *a->index() == *b->index();
        default: never();
    }
}

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
    expect(!base.has_fragment());
     // Serialize the top-level item as just "#".  This technically results in a
     // different IRI, but within AYU, an empty fragment is always considered
     // equivalent to no fragment.  TODO: Actually, we should probably serialize
     // all references with a fragment no matter what.
    if (iri == base) return AnyString::Static("#");
    else return iri.spec_relative_to(base);
}

IRI location_iri_from_relative_iri (Str rel) {
    if (!rel) return IRI();
    auto base = location_to_iri(current_root_location());
    expect(!base.has_fragment());
     // Faster to chop off empty fragment before parsing.  This may cause a
     // different result if there are multiple #s in the string, which would
     // result in an invalid IRI, but I don't think that's likely to matter.
    if (rel == "#") return base;
    else if (rel.back() == '#') rel = rel.slice(0, rel.size()-1);
    auto r = IRI(rel, base);
    if (!r) throw GenericError("Invalid IRI ", r.possibly_invalid_spec());
    return r;
}

IRI location_to_iri (LocationRef loc) {
    if (!*loc) return IRI();
    UniqueString fragment;
    for (;; loc = LocationRef(*loc->parent())) {
        expect(loc->data);
        switch (loc->data->form) {
            case RESOURCE: {
                auto res = *loc->resource();
                if (!fragment) return res.name();
                else return IRI(cat('#', fragment), res.name());
            }
            case REFERENCE: {
                 // TODO: This might be incorrect in some cases.  Check it
                 // against current_root_location() maybe?
                static const IRI anon ("ayu-current-root:");
                if (!fragment) return anon;
                else return IRI(cat('#', fragment), anon);
            }
            case KEY: {
                Str key = *loc->key();
                UniqueString segment;
                if (!key || key[0] == '\'' || std::isdigit(key[0])) {
                    segment = cat('\'', iri::encode(key));
                }
                else segment = iri::encode(key);
                if (!fragment) fragment = move(segment);
                else fragment = cat(move(segment), '/', move(fragment));
                break;
            }
            case INDEX: {
                usize index = *loc->index();
                if (!fragment) fragment = cat(index);
                else fragment = cat(index, '/', move(fragment));
                break;
            }
            default: never();
        }
    }
}

Location location_from_iri (const IRI& iri) {
    if (!iri) return Location();
    auto root_iri = iri.iri_without_fragment();
    Location r;
    if (root_iri == "ayu-current-root:") {
        r = current_root_location();
    }
    else r = Location(Resource(root_iri));
    Str fragment = iri.fragment();
    if (!fragment.empty()) {
        usize segment_start = 0;
        bool segment_is_string = false;
        for (usize i = 0; i < fragment.size()+1; i++) {
            switch (i == fragment.size() ? '/' : fragment[i]) {
                case '/': {
                    Str segment = fragment.substr(
                        segment_start, i - segment_start
                    );
                    if (segment_is_string) {
                        r = Location(r, iri::decode(segment));
                    }
                    else if (segment.size() == 0) {
                         // Ignore
                    }
                    else {
                        usize index = -1;
                        auto [ptr, ec] = std::from_chars(
                            segment.begin(), segment.end(), index
                        );
                        if (ptr == 0) {
                            throw GenericError("Index segment too big?");
                        }
                        r = Location(r, index);
                    }
                    segment_start = i+1;
                    segment_is_string = false;
                    break;
                }
                case '\'': {
                    if (i == segment_start && !segment_is_string) {
                        segment_start = i+1;
                    }
                    segment_is_string = true;
                    break;
                }
                case ANY_DECIMAL_DIGIT: break;
                default: segment_is_string = true; break;
            }
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

    auto loc = location_from_iri(IRI("ayu-test:/#bar/1/bu%2Fp//33/0/'3/''/'//"));
    const Location* l = &loc;
    is(*l->key(), "", "Empty key");
    l = l->parent();
    is(*l->key(), "'", "Key with apostrophe");
    l = l->parent();
    is(*l->key(), "3", "Number-like key");
    l = l->parent();
    is(*l->index(), 0u, "Index 0");
    l = l->parent();
    is(*l->index(), 33u, "Index 33");
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
