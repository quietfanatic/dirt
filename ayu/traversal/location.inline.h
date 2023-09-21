#pragma once

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
 // ResourceLocation is extern to break a cyclic dependency.
struct ReferenceLocation : LocationData {
    Reference reference;
    ReferenceLocation (MoveRef<Reference> ref) :
        LocationData(REFERENCE), reference(*move(ref))
    { }
};
struct KeyLocation : LocationData {
    Location parent;
    AnyString key;
    KeyLocation (MoveRef<Location> p, MoveRef<AnyString> k) :
        LocationData(in::KEY), parent(*move(p)), key(*move(k))
    { }
};
struct IndexLocation : LocationData {
    Location parent;
    uint32 index;
    IndexLocation (MoveRef<Location> p, usize i) :
        LocationData(in::INDEX), parent(*move(p)), index(i)
    { expect(index == i); }
};

};

inline Location::Location (const Reference& ref) noexcept :
    data(new in::ReferenceLocation(ref))
{ }
inline Location::Location (MoveRef<Location> p, MoveRef<AnyString> k) noexcept :
    data(new in::KeyLocation(expect(*move(p)), *move(k)))
{ }
inline Location::Location (MoveRef<Location> p, usize i) noexcept :
    data(new in::IndexLocation(expect(*move(p)), i))
{ }

inline const Reference* Location::reference () const noexcept {
    switch (data->form) {
        case in::REFERENCE:
            return &static_cast<in::ReferenceLocation*>(data.p)->reference;
        default: return null;
    }
}

inline const Location* Location::parent () const noexcept {
    switch (data->form) {
        case in::KEY: return &static_cast<in::KeyLocation*>(data.p)->parent;
        case in::INDEX: return &static_cast<in::IndexLocation*>(data.p)->parent;
        default: return null;
    }
}
inline const AnyString* Location::key () const noexcept {
    switch (data->form) {
        case in::KEY: return &static_cast<in::KeyLocation*>(data.p)->key;
        default: return null;
    }
}
inline const uint32* Location::index () const noexcept {
    switch (data->form) {
        case in::INDEX: return &static_cast<in::IndexLocation*>(data.p)->index;
        default: return null;
    }
}

inline Location Location::root () const noexcept {
    const Location* l = this;
    while (l->parent()) l = l->parent();
    return *l;
}

} // ayu
