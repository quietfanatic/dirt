// This contains some utilities for accessing data through AnyRefs (and internal
// accessor objects).  You likely do not need to use anything in this file.

#pragma once
#include "../../uni/callback-ref.h"
#include "../common.h"

namespace ayu {

 // This is a bitfield that communicates what kind of access is possible for a
 // reference, and what kind of access is requested for an access.
enum class AccessCaps : u8 {
     // Request/allow access to either the original item or a
     // default-constructed value which will be written back to the item after
     // the callback.  Neglecting to write to it in the callback may clear the
     // item.
     //
     // This is bit 1 to match bit 1 of AnyPtr.
    Write = 0x1,
     // Request/allow access to either the original item or a copy that will go
     // out of scope after the callback.  This should always be set on
     // accessors.
    Read = 0x2,
     // Request/allow access to either the original item or a copy which will be
     // written back after the callback.  May be implemented by a
     // read-modify-write sequence.  When doing nested write access, you must
     // use this instead of Write on all but the lowest level of access, so that
     // other parts of the outer items don't get cleared.
    Modify = Write|Read,
     // Request/allow access to the permanent address of the item.
    Address = 0x4,
     // If we had these, this is where they would go, but we don't.
    //WriteChildren = 0x10,
    //ReadChildren = 0x20,
     // Allow children to be addressable even if this item isn't addressable.
     // On accessors, this should always be set if Address is set.  This should
     // never be set when requesting access.  This value should be far enough
     // away to shift into Address without affecting other bits.
    AddressChildren = 0x40,

    AllowEverything = Write|Read|Address|AddressChildren,
};
DECLARE_ENUM_BITWISE_OPERATORS(AccessCaps)
using AC = AccessCaps;

 // Check if one set of capabilities fully contains another.  This can be used
 // to check if a mode is allowed by caps, or if one set of caps is stricter
 // than another.
constexpr bool contains (AccessCaps more, AccessCaps less) {
    return !(less & ~more); // See if less has any bits that aren't set in more
}

 // This is how capabilities combine when you're doing nested access.
constexpr AccessCaps operator* (AccessCaps outer, AccessCaps inner) {
     // Shift by 4 to merge the ChildrenAddressable bit into the Addressable
     // bit.
    return (outer | outer >> 4) & inner;
}

 // This is a callback passed to access operations.  The parameters are:
 //   - type: the type of the item being accessed.
 //   - address: a pointer to either the item being accessed or a temporary that
 //     represents it (check caps & AC::Addressable to tell the difference).
using AccessCB = CallbackRef<void(Type, Mu*)>;

 // Some accessor constraints.

template <class Acr, class From>
concept AccessorFrom = std::is_same_v<typename Acr::AcrFromType, From>;

template <class Acr, class To>
concept AccessorTo = std::is_same_v<typename Acr::AcrToType, To>;

template <class Acr, class From, class To>
concept AccessorFromTo = AccessorFrom<Acr, From> && AccessorTo<Acr, To>;

} // ayu
