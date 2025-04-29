// This contains some utilities for accessing data through AnyRefs (and internal
// accessor objects).  You likely do not need to use anything in this file.

#pragma once
#include "../../uni/callback-ref.h"
#include "../common.h"

namespace ayu {

 // This is a tag communicating what kind of access is desired.  TODO: Merge
 // this with AccessCaps.
enum class AccessMode {
     // Requests access to either the original item or a copy that will go out
     // of scope after the callback.  Access operations to get the address of an
     // item should use AM::Read.  TODO: provide AM::Address.
    Read = 0x1,
     // Requests access to either the original item or a default-constructed
     // value which will be written back to the item after the callback.
     // Neglecting to write to it in the callback may clear the item.
    Write = 0x2,
     // Requests access to either the original item or a copy which will be
     // written back after the callback.  May be implemented by a
     // read-modify-write sequence.
    Modify = 0x0
};
using AM = AccessMode;

constexpr
bool valid_access_mode (AccessMode mode) { return u8(mode) <= 0x2; }

 // For nested access routines, if you're doing a write operation, you need to
 // convert all but the last writes into modifies, otherwise other parts of the
 // object may be overwritten.
constexpr
AccessMode write_to_modify (AccessMode mode) {
     // Use the weird values we picked above to optimize this common operation.
     // TODO: switch to bitfields and & in Read instead of this.
    return AccessMode(int(mode) & ~int(AM::Write));
}

 // Flagset for what types of accesses are allowed in a given situation
enum class AccessCaps : u8 {
     // Allow writing.  If this isn't set, the access is readonly.
    Writeable = 0x1,
     // Allow taking the address the item being accessed.  If this isn't set,
     // the accessed item may be manifested by a temporary that will go out of
     // scope when the access finishes.
    Addressable = 0x2,
     // Allow addressing this item's immediate children (if they're addressable
     // themselves), even if this item isn't addressable.  If Addressable is
     // true, this must also be true.
     //
     // Yes, we're skipping 0x4 intentionally for an optimization.
    ChildrenAddressable = 0x8,

    Everything = Writeable | Addressable | ChildrenAddressable
};
DECLARE_ENUM_BITWISE_OPERATORS(AccessCaps)
using AC = AccessCaps;

 // This is how capabilities combine when you're doing nested access.  It'd be
 // nice to give this lower precendence than | and & but that's not feasible.
constexpr AccessCaps operator* (AccessCaps outer, AccessCaps inner) {
     // Shift by 2 to merge the ChildrenAddressable bit into the Addressable
     // bit.
    return (outer | outer >> 2) & inner;
}

 // Check writeability
constexpr
bool caps_allow_mode (AccessCaps caps, AccessMode mode) {
    return (mode == AM::Read) | !!(caps & AC::Writeable);
}

 // This is a callback passed to access operations.  The parameters are:
 //   - type: the type of the item being accessed.
 //   - address: a pointer to either the item being accessed or a temporary that
 //     represents it (check caps & AC::Addressable to tell the difference).
using AccessCB = CallbackRef<void(Type, Mu*, AccessCaps)>;

 // Some accessor constraints.  TODO: Move these to describe-base.h

template <class Acr, class From>
concept AccessorFrom = std::is_same_v<typename Acr::AcrFromType, From>;

template <class Acr, class To>
concept AccessorTo = std::is_same_v<typename Acr::AcrToType, To>;

template <class Acr, class From, class To>
concept AccessorFromTo = AccessorFrom<Acr, From> && AccessorTo<Acr, To>;

} // ayu
