// This contains some utilities for accessing data through AnyRefs (and internal
// accessor objects).  You likely do not need to use anything in this file.

#pragma once
#include "../../uni/callback-ref.h"
#include "anyptr.h"

namespace ayu {

 // This is the callback passed to access operations.
using AccessCB = CallbackRef<void(AnyPtr, bool)>;

 // This is the tag communicating what kind of access is desired.
enum class AccessMode {
     // Requests an AnyPtr to either the original item or a copy that will go
     // out of scope after the callback.  The AnyPtr will only be readonly if
     // the item's type is const.  You should not write to this; writes to the
     // AnyPtr may or may not be written to the item
    Read = 0x1,
     // Requests an AnyPtr to either the original item or a default-constructed
     // value which will be written back to the item after the callback.
     // Neglecting to write to the AnyPtr in the callback may clear the object.
    Write = 0x2,
     // Requests an AnyPtr to either the original item or a copy which will be
     // written back after the callback.  May be implemented by a
     // read-modify-write sequence.
    Modify = 0x0
};

 // Use the weird values we selected to optimize this common operation
inline
AccessMode write_to_modify (AccessMode mode) {
    return AccessMode(int(mode) & ~int(AccessMode::Write));
}

} // ayu
