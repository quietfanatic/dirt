#pragma once

namespace ayu {
namespace in {

 // ResourceLocation is extern to break a cyclic dependency.
struct ReferenceLocation : Location {
    Reference reference;
    ReferenceLocation (MoveRef<Reference> ref) :
        Location(REFERENCE), reference(*move(ref))
    { }
};
struct KeyLocation : Location {
    SharedLocation parent;
    AnyString key;
    KeyLocation (MoveRef<SharedLocation> p, MoveRef<AnyString> k) :
        Location(KEY), parent(*move(p)), key(*move(k))
    { }
};
struct IndexLocation : Location {
    SharedLocation parent;
    uint32 index;
    IndexLocation (MoveRef<SharedLocation> p, usize i) :
        Location(INDEX), parent(*move(p)), index(i)
    { expect(index == i); }
};

};

inline SharedLocation::SharedLocation (const Reference& ref) noexcept :
    data(new in::ReferenceLocation(ref))
{ }
inline SharedLocation::SharedLocation (MoveRef<SharedLocation> p, MoveRef<AnyString> k) noexcept :
    data(new in::KeyLocation(expect(*move(p)), *move(k)))
{ }
inline SharedLocation::SharedLocation (MoveRef<SharedLocation> p, usize i) noexcept :
    data(new in::IndexLocation(expect(*move(p)), i))
{ }

inline const Reference* Location::reference () const noexcept {
    switch (form) {
        case REFERENCE:
            return &static_cast<const in::ReferenceLocation*>(this)->reference;
        default: return null;
    }
}

inline LocationRef Location::parent () const noexcept {
    switch (form) {
        case KEY: return static_cast<const in::KeyLocation*>(this)->parent;
        case INDEX: return static_cast<const in::IndexLocation*>(this)->parent;
        default: return {};
    }
}
inline const AnyString* Location::key () const noexcept {
    switch (form) {
        case KEY: return &static_cast<const in::KeyLocation*>(this)->key;
        default: return null;
    }
}
inline const uint32* Location::index () const noexcept {
    switch (form) {
        case INDEX: return &static_cast<const in::IndexLocation*>(this)->index;
        default: return null;
    }
}

inline LocationRef Location::root () const noexcept {
    auto l = LocationRef(this);
    while (l->parent()) l = l->parent();
    return l;
}

} // ayu
