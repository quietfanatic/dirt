// This file contains operations that might require scanning large amounts of
// program data.

#pragma once
#include "../common.h"
#include "../reflection/anyref.h"
#include "../reflection/type.h"
#include "route.h"

namespace ayu {

 // Convert an AnyPtr to a Route.  This will be slow by itself, since it
 // must scan all loaded resources.  If a KeepRouteCache object is alive, the
 // first call to find_pointer will build a map of AnyPtrs to Routes,
 // and subsequent calls to find_pointer will be as fast as a hash lookup.
 // Returns the empty Route if the pointer was not found or if a null pointer
 // was passed.
SharedRoute find_pointer (AnyPtr);
 // Same as above, but find a AnyRef.  Equivalent to the above if the
 // AnyRef is addressable.  If the AnyRef is not addressable, this may
 // fail since references with dynamically generated Accessors may not be
 // comparable.  Returns the empty AnyRef if the reference was not found or
 // if a null reference was passed.
SharedRoute find_reference (const AnyRef&);

 // These are the same as find_*, except they'll throw AnyRefNotFound
 // if the provided AnyPtr/AnyRef was not found (and is not null)
SharedRoute pointer_to_route (AnyPtr);
SharedRoute reference_to_route (const AnyRef&);

 // While this is alive, a cache mapping pointers to routes will be kept, making
 // find_pointer and find_reference faster.  Do not modify any program data
 // while keeping the route cache, since there is no way for the cache to stay
 // up-to-date.
struct KeepRouteCache {
    KeepRouteCache () noexcept;
    ~KeepRouteCache ();
};

 // While this is alive, if find_pointer() or find_reference() is called with
 // this reference, skip the scanning process and return this route.  These must
 // only be destroyed in first-in-last-out order, which will be fine if you only
 // construct them on the stack
struct PushLikelyRef {
    PushLikelyRef (AnyRef, SharedRoute) noexcept;
    ~PushLikelyRef ();

    AnyRef reference;
    SharedRoute route;
    PushLikelyRef* next;
};

using ScanPointersCB = CallbackRef<bool(AnyPtr, RouteRef)>;
using ScanReferencesCB = CallbackRef<bool(const AnyRef&, RouteRef)>;

///// Scanning operations
 // You probably don't need to use these directly, but you can if you want.  The
 // route cache does not accelerate these functions.  These currently do a
 // depth-first search, but they may do a breadth-first search in the future.

 // Scans all visible addressable items under the given address of the given
 // type.  Skips unaddressable items, and the children of unaddressable items
 // that don't have pass_through_addressable.
 //   base_item: AnyPtr to the item to start scanning at.
 //   base_rt: Route to the base item, or {} if you don't care.
 //   cb: Is called for each addressable item with its pointer and route
 //     (based on base_rt).  The callback is called for parent items before
 //     their child items and is first called with (base_item, base_rt) before
 //     any scanning.  If an item only has a delegate() descriptor, the callback
 //     will be called both for the parent item and the child item with the same
 //     route.  If the callback returns true, the scan will be stopped.
 //   returns: true if the callback ever returned true.
bool scan_pointers (
    AnyPtr base_item, RouteRef base_rt, ScanPointersCB cb
);

 // Scans all visible items under the given reference, whether or not they are
 // addressable.  Skips items with the no_refs_to_children.
 //   base_item: AnyRef to the item to start scanning at.
 //   base_rt: Route to the base item, or {} if you don't care.
 //   cb: Is called for each item with a reference to it and its route (based
 //     on base_rt).  The callback is called for parent items before their
 //     child items and is first called with (base_item, base_rt) before any
 //     scanning.  If an item only has a delegate() descriptor, the callback
 //     will be called both for the parent item and the child item with the same
 //     route.  If the callback returns true, the scan will be stopped.
 //   returns: true if the callback ever returned true.
bool scan_references (
    const AnyRef& base_item, RouteRef base_rt, ScanReferencesCB cb
);

 // What it says.  This is used internally for error reporting.
bool scan_references_ignoring_no_refs_to_children (
    const AnyRef& base_item, RouteRef base_rt, ScanReferencesCB cb
);

 // Scan under a particular resource's data.  The route is automatically
 // determined from the resource's name.  This silently does nothing and returns
 // false if the resource's state is RS::Unloaded.
bool scan_resource_pointers (ResourceRef res, ScanPointersCB cb);
bool scan_resource_references (ResourceRef res, ScanReferencesCB cb);
 // Scan all loaded resources.
bool scan_universe_pointers (ScanPointersCB cb);
bool scan_universe_references (ScanReferencesCB cb);

 // This is true while there is an ongoing scan.  While this is true, you cannot
 // start a new scan.
extern bool currently_scanning;

 // reference_to_route or pointer_to_route failed to find the AnyRef.
constexpr ErrorCode e_ReferenceNotFound = "ayu::e_ReferenceNotFound";
 // Tried to start a new scan while there's still a scan going.
constexpr ErrorCode e_ScanWhileScanning = "ayu::e_ScanWhileScanning";

///// INLINES

inline PushLikelyRef* first_plr = null;

inline PushLikelyRef::PushLikelyRef (
    AnyRef r, SharedRoute l
) noexcept :
    reference(move(r)), route(move(l)), next(first_plr)
{
#ifndef NDEBUG
    require(reference_from_route(route) == reference);
#endif
    first_plr = this;
}
inline PushLikelyRef::~PushLikelyRef () {
    expect(first_plr == this);
    first_plr = next;
}

} // namespace ayu
