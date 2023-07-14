#pragma once

#include <charconv>
#include "../reference.h"
#include "../resource.h"

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
    uint32 index;
    IndexLocation (Location p, usize i) :
        LocationData(INDEX), parent(move(p)), index(i)
    { expect(index == i); }
};

} using namespace in;

inline Location::Location (Resource res) :
    data(new ResourceLocation(move(res)))
{ }
inline Location::Location (Reference ref) :
    data(new ReferenceLocation(move(ref)))
{ }
inline Location::Location (Location p, AnyString k) :
    data(new KeyLocation(expect(move(p)), move(k)))
{ }
inline Location::Location (Location p, usize i) :
    data(new IndexLocation(expect(move(p)), i))
{ }

inline const Resource* Location::resource () const {
    switch (data->form) {
        case RESOURCE: return &static_cast<ResourceLocation*>(data.p)->resource;
        default: return null;
    }
}

inline const Reference* Location::reference () const {
    switch (data->form) {
        case REFERENCE: return &static_cast<ReferenceLocation*>(data.p)->reference;
        default: return null;
    }
}

inline const Location* Location::parent () const {
    switch (data->form) {
        case KEY: return &static_cast<KeyLocation*>(data.p)->parent;
        case INDEX: return &static_cast<IndexLocation*>(data.p)->parent;
        default: return null;
    }
}
inline const AnyString* Location::key () const {
    switch (data->form) {
        case KEY: return &static_cast<KeyLocation*>(data.p)->key;
        default: return null;
    }
}
inline const uint32* Location::index () const {
    switch (data->form) {
        case INDEX: return &static_cast<IndexLocation*>(data.p)->index;
        default: return null;
    }
}

inline Location Location::root () const {
    const Location* l = this;
    while (l->parent()) l = l->parent();
    return *l;
}

} // ayu
