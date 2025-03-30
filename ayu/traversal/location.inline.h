#pragma once

namespace ayu {
namespace in {

 // ResourceLocation is extern to break a cyclic dependency.
struct ReferenceLocation : Location {
    AnyRef reference;
    ReferenceLocation (AnyRef ref) :
        Location(LF::Reference), reference(move(ref))
    { expect(reference); }
};
struct KeyLocation : Location {
    SharedLocation parent;
    AnyString key;
    KeyLocation (SharedLocation p, AnyString k) :
        Location(LF::Key), parent(move(p)), key(move(k))
    { expect(parent); }
};
struct IndexLocation : Location {
    SharedLocation parent;
    u32 index;
    IndexLocation (SharedLocation p, u32 i) :
        Location(LF::Index), parent(move(p)), index(i)
    { expect(parent); }
};

};

inline SharedLocation::SharedLocation (const AnyRef& ref) noexcept :
    data(new in::ReferenceLocation(ref))
{ }
inline SharedLocation::SharedLocation (SharedLocation p, AnyString k) noexcept :
    data(new in::KeyLocation(move(p), move(k)))
{ }
inline SharedLocation::SharedLocation (SharedLocation p, u32 i) noexcept :
    data(new in::IndexLocation(move(p), i))
{ }

inline const AnyRef* Location::reference () const noexcept {
    switch (form) {
        case LF::Reference:
            return &static_cast<const in::ReferenceLocation*>(this)->reference;
        default: return null;
    }
}

inline LocationRef Location::parent () const noexcept {
    switch (form) {
        case LF::Key: return static_cast<const in::KeyLocation*>(this)->parent;
        case LF::Index:
            return static_cast<const in::IndexLocation*>(this)->parent;
        default: return {};
    }
}
inline const AnyString* Location::key () const noexcept {
    switch (form) {
        case LF::Key: return &static_cast<const in::KeyLocation*>(this)->key;
        default: return null;
    }
}
inline const u32* Location::index () const noexcept {
    switch (form) {
        case LF::Index:
            return &static_cast<const in::IndexLocation*>(this)->index;
        default: return null;
    }
}

inline LocationRef Location::root () const noexcept {
    auto l = LocationRef(this);
    while (l->parent()) l = l->parent();
    return l;
}

} // ayu
