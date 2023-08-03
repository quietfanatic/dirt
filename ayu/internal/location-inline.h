#pragma once

#include <charconv>

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
 // ResourceLocation and ReferenceLocation are extern to break a cyclic
 // dependency.
struct KeyLocation : LocationData {
    Location parent;
    AnyString key;
    KeyLocation (MoveRef<Location> p, MoveRef<AnyString> k) :
        LocationData(KEY), parent(*move(p)), key(*move(k))
    { }
};
struct IndexLocation : LocationData {
    Location parent;
    uint32 index;
    IndexLocation (MoveRef<Location> p, usize i) :
        LocationData(INDEX), parent(*move(p)), index(i)
    { expect(index == i); }
};

} using namespace in;

inline Location::Location (Location p, AnyString k) noexcept :
    data(new KeyLocation(expect(move(p)), move(k)))
{ }
inline Location::Location (Location p, usize i) noexcept :
    data(new IndexLocation(expect(move(p)), i))
{ }

inline const Location* Location::parent () const noexcept {
    switch (data->form) {
        case KEY: return &static_cast<KeyLocation*>(data.p)->parent;
        case INDEX: return &static_cast<IndexLocation*>(data.p)->parent;
        default: return null;
    }
}
inline const AnyString* Location::key () const noexcept {
    switch (data->form) {
        case KEY: return &static_cast<KeyLocation*>(data.p)->key;
        default: return null;
    }
}
inline const uint32* Location::index () const noexcept {
    switch (data->form) {
        case INDEX: return &static_cast<IndexLocation*>(data.p)->index;
        default: return null;
    }
}

inline Location Location::root () const noexcept {
    const Location* l = this;
    while (l->parent()) l = l->parent();
    return *l;
}

} // ayu
