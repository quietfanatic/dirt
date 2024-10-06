#pragma once

namespace ayu {
namespace in {

 // ResourceLocation is extern to break a cyclic dependency.
struct ReferenceLocation : Location {
    AnyRef reference;
    ReferenceLocation (MoveRef<AnyRef> ref) :
        Location(LF::Reference), reference(*move(ref))
    { }
};
struct KeyLocation : Location {
    SharedLocation parent;
    AnyString key;
    KeyLocation (MoveRef<SharedLocation> p, MoveRef<AnyString> k) :
        Location(LF::Key), parent(*move(p)), key(*move(k))
    { }
};
struct IndexLocation : Location {
    SharedLocation parent;
    u32 index;
    IndexLocation (MoveRef<SharedLocation> p, u32 i) :
        Location(LF::Index), parent(*move(p)), index(i)
    { expect(index == i); } // forgot what this is for but I think it's optimization
};

};

inline SharedLocation::SharedLocation (const AnyRef& ref) noexcept :
    data(new in::ReferenceLocation(ref))
{ }
inline SharedLocation::SharedLocation (MoveRef<SharedLocation> p, MoveRef<AnyString> k) noexcept :
    data(new in::KeyLocation(expect(*move(p)), *move(k)))
{ }
inline SharedLocation::SharedLocation (MoveRef<SharedLocation> p, u32 i) noexcept :
    data(new in::IndexLocation(expect(*move(p)), i))
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
