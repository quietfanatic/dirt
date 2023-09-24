// This file contains operations that might require scanning large amounts of
// program data.

#pragma once
#include "../common.h"
#include "../reflection/type.h"

namespace ayu {

 // Convert a Pointer to a Location.  This will be slow by itself, since it
 // must scan all loaded resources.  If a KeepLocationCache object is alive, the
 // first call to find_pointer will build a map of Pointers to Locations,
 // and subsequent calls to find_pointer will be as fast as a hash lookup.
 // Returns the empty Location if the pointer was not found or if a null pointer
 // was passed.
Location find_pointer (Pointer);
 // Same as above, but find a Reference.  Equivalent to the above if the
 // Reference is addressable.  If the Reference is not addressable, this may
 // fail since references with dynamically generated Accessors may not be
 // comparable.  Returns the empty Reference if the reference was not found or
 // if a null reference was passed.
Location find_reference (const Reference&);

 // These are the same as find_*, except they'll throw ReferenceNotFound
 // if the provided Pointer/Reference was not found (and is not null)
Location pointer_to_location (Pointer);
Location reference_to_location (const Reference&);

 // While this is alive, a cache mapping pointers to locations will be kept,
 // making find_pointer and find_reference faster.  Do not modify any program
 // data while keeping the location cache, since there is no way for the cache
 // to stay up-to-date.
struct KeepLocationCache {
    KeepLocationCache () noexcept;
    ~KeepLocationCache ();
};

///// Scanning operations
 // You probably don't need to use these directly, but you can if you want.  The
 // location cache does not accelerate these functions.  These currently do a
 // depth-first search, but they may do a breadth-first search in the future.

 // Scans all visible addressable items under the given address of the given
 // type.  Skips unaddressable items, and the children of unaddressable items
 // that don't have pass_through_addressable.
 //   base_item: Pointer to the item to start scanning at.
 //   base_loc: Location of the base item, or Location() if you don't care.
 //   cb: Is called for each addressable item with its pointer and location
 //     (based on base_loc).  The callback is called for parent items before
 //     their child items and is first called with (base_item, base_loc) before
 //     any scanning.  If an item only has a delegate() descriptor, the callback
 //     will be called both for the parent item and the child item with the same
 //     location.
void scan_pointers (
    Pointer base_item, LocationRef base_loc,
    CallbackRef<void(Pointer, LocationRef)> cb
);

 // Scans all visible items under the given reference, whether or not they are
 // addressable.
 //   base_item: Reference to the item to start scanning at.
 //   base_loc: Location of the base item, or Location() if you don't care.
 //   cb: Is called for each item with a reference to it and its location (based
 //     on base_loc).  The callback is called for parent items before their
 //     child items and is first called with (base_item, base_loc) before any
 //     scanning.  If an item only has a delegate() descriptor, the callback
 //     will be called both for the parent item and the child item with the same
 //     location.
void scan_references (
    const Reference& base_item, LocationRef base_loc,
    CallbackRef<void(const Reference&, LocationRef)> cb
);

 // Scan under a particular resource's data.  The location is automatically
 // determined from the resource's name.  This silently does nothing and returns
 // false if the resource's state is UNLOADED.
void scan_resource_pointers (
    const Resource& res, CallbackRef<void(Pointer, LocationRef)> cb
);
void scan_resource_references (
    const Resource& res, CallbackRef<void(const Reference&, LocationRef)> cb
);
 // Scan all loaded resources.
void scan_universe_pointers (
    CallbackRef<void(Pointer, LocationRef)> cb
);
void scan_universe_references (
    CallbackRef<void(const Reference&, LocationRef)> cb
);

 // reference_to_location or pointer_to_location failed to find the Reference.
constexpr ErrorCode e_ReferenceNotFound = "ayu::e_ReferenceNotFound";

} // namespace ayu
