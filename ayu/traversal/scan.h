// This file contains operations that might require scanning large amounts of
// program data.

#pragma once
#include "../common.h"
#include "../reflection/link.h"
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
 // Same as above, but find a Link.  Equivalent to the above if the Link is
 // addressable.  If the Link is not addressable, this may fail since links with
 // dynamically generated Accessors may not be comparable.  Returns the empty
 // Link if the item was not found or if a null or empty link was passed.
SharedRoute find_link (const Link&);

 // These are the same as find_*, except they'll throw LinkNotFound if the
 // provided AnyPtr/Link was not found (and is not null)
SharedRoute pointer_to_route (AnyPtr);
SharedRoute link_to_route (const Link&);

 // While this is alive, a cache mapping pointers to routes will be kept, making
 // find_pointer and find_link faster.  Do not modify any program data while
 // keeping the route cache, since there is no way for the cache to stay
 // up-to-date.
struct KeepRouteCache {
    KeepRouteCache ();
    ~KeepRouteCache ();
};

 // While this is alive, if find_pointer() or find_link() is called with
 // this link, skip the scanning process and return this route.  These must only
 // be destroyed in first-in-last-out order, which will be fine if you only
 // construct them on the stack
struct PushLikelyLink {
    PushLikelyLink (Link, SharedRoute);
    ~PushLikelyLink ();

    Link link;
    SharedRoute route;
    PushLikelyLink* next;
};

using ScanPointersCB = CallbackRef<bool(AnyPtr, RouteRef)>;
using ScanLinksCB = CallbackRef<bool(const Link&, RouteRef)>;

///// Scanning operations
 // You probably don't need to use these directly, but you can if you want.  The
 // route cache does not accelerate these functions.  These currently do a
 // depth-first search, but they may do a breadth-first search in the future.

 // Scans all visible addressable items under the given address of the given
 // type.  Skips unaddressable items and the children of unaddressable items
 // that don't have children_addressable.
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

 // Scans all visible items under the given link, whether or not they are
 // addressable.  Skips items with the no_links_to_children.
 //   base_item: Link to the item to start scanning at.
 //   base_rt: Route to the base item, or {} if you don't care.
 //   cb: Is called for each item with a reference to it and its route (based
 //     on base_rt).  The callback is called for parent items before their
 //     child items and is first called with (base_item, base_rt) before any
 //     scanning.  If an item only has a delegate() descriptor, the callback
 //     will be called both for the parent item and the child item with the same
 //     route.  If the callback returns true, the scan will be stopped.
 //   returns: true if the callback ever returned true.
bool scan_links (
    const Link& base_item, RouteRef base_rt, ScanLinksCB cb
);

 // What it says.  This is used internally for error reporting.
bool scan_links_ignoring_no_links_to_children (
    const Link& base_item, RouteRef base_rt, ScanLinksCB cb
);

 // Scan under a particular resource's data.  The route is automatically
 // determined from the resource's name.  This silently does nothing and returns
 // false if the resource's state is RS::Unloaded.
bool scan_resource_pointers (ResourceRef res, ScanPointersCB cb);
bool scan_resource_links (ResourceRef res, ScanLinksCB cb);
 // Scan all loaded resources.
bool scan_universe_pointers (ScanPointersCB cb);
bool scan_universe_links (ScanLinksCB cb);

 // This is true while there is an ongoing scan.  While this is true, you cannot
 // start a new scan or perform any resource operations.
extern bool currently_scanning;

 // link_to_route or pointer_to_route failed to find the Link.
constexpr ErrorCode e_LinkNotFound = "ayu::e_LinkNotFound";
 // Tried to do something you can't do during a scan.
constexpr ErrorCode e_ForbiddenWhileScanning = "ayu::e_ForbiddenWhileScanning";

///// INLINES

inline PushLikelyLink* first_pll = null;

inline PushLikelyLink::PushLikelyLink (
    Link l, SharedRoute r
) :
    link(move(l)), route(move(r)), next(first_pll)
{
#ifndef NDEBUG
    require(link_from_route(route) == link);
#endif
    first_pll = this;
}
inline PushLikelyLink::~PushLikelyLink () {
    assume(first_pll == this);
    first_pll = next;
}

namespace in {
    inline u32 keep_route_cache_count = 0;
    void clear_route_cache ();
}


inline KeepRouteCache::KeepRouteCache () {
    in::keep_route_cache_count++;
}
inline KeepRouteCache::~KeepRouteCache () {
    if (!--in::keep_route_cache_count) in::clear_route_cache();
}

} // namespace ayu
