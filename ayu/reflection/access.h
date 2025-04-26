// This contains some utilities for accessing data through AnyRefs (and internal
// accessor objects).  You likely do not need to use anything in this file.

#pragma once
#include "../../uni/callback-ref.h"
#include "anyptr.h"

namespace ayu {

 // This is a callback passed to access operations.  The first argument is a
 // pointer to the item being accessed or a temporary copy.  The second argument
 // says whether the item is addressable:
 //   - false: The AnyPtr is only valid for the duration of the callback.
 //   - true: The AnyPtr can safely be copied out of the callback.
using AccessCB = CallbackRef<void(AnyPtr, bool)>;

 // This is a tag communicating what kind of access is desired.
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

 // For nested access routines, if you're doing a write operation, you need to
 // convert all but the last writes into modifies, otherwise other parts of the
 // object may be overwritten.
inline
AccessMode write_to_modify (AccessMode mode) {
     // Use the weird values we picked above to optimize this common operation.
    return AccessMode(int(mode) & ~int(AccessMode::Write));
}

 // This is the base class for accessors.  Every function in the "ACCESSORS"
 // section of describe-base.h returns a subclass of this, although the details
 // of those subclasses are not public.
 //
 // In some cases, accessors may be refcounted, so don't duplicate pointers to
 // them.
namespace in { struct Accessor; }
using in::Accessor;

 // This checks that the given accessor subclass is an accessor for the given
 // type (specifically, an accessor whose from-type is the given type).
template <class Acr, class From>
concept AccessorFor = std::is_same_v<typename Acr::AcrFromType, From>;

} // ayu
