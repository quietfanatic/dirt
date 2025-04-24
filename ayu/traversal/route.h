// A Route is an intermediate step between an AnyRef and an IRI.  A valid
// Route can be easily converted to and from a valid IRI.  A Route can
// also be easily converted to a AnyRef, but converting a AnyRef to a
// Route may require scanning a lot of data.  The functions for doing these
// conversions are in scan.h.
//
// You shouldn't have to use this class directly, but I guess you can if you
// want to.
//
// Internally, a Route is a recursive object that is a symbolic representation
// of a AnyRef, explaining how to reach the referend from the root Resource by a
// chain of item_attr() and item_elem() calls.  In ADT syntax, it looks like
//
//     data Route = ResourceRoute Resource
//                | ReferenceRoute AnyRef
//                | KeyRoute Route AnyString
//                | IndexRoute Route u32
//
// Normally an object like this would be called a Path, but that risks confusion
// with the path component of an IRI.

#pragma once
#include "../../iri/iri.h"
#include "../../uni/lilac.h"
#include "../common.internal.h"
#include "../reflection/anyref.h"

namespace ayu {

enum class RouteForm : u8 {
    Resource,
    Reference,
    Key,
    Index,
};
using RF = RouteForm;

 // The abstract interface for Routes.
struct Route : in::RefCounted {
    RouteForm form;
     // Returns empty if this is not a resource root.
    ResourceRef resource () const noexcept;
     // Returns null if this is not a reference root.
    const AnyRef* reference () const noexcept;
     // Returns empty if this is a root.
    RouteRef parent () const noexcept;
     // Returns null if this Route is a root or has an index.
    const AnyString* key () const noexcept;
     // Returns null if this Route is a root or has a key.
    const u32* index () const noexcept;

     // Walks down to the root Route (containing either a Resource or a
     // AnyRef) and returns it.
    RouteRef root () const noexcept;

    static void* operator new (usize s) {
        return lilac::allocate_fixed_size(s);
    }
    static void operator delete (void* p, usize s) {
        lilac::deallocate_fixed_size(p, s);
    }
    protected:
    Route (RouteForm f) : form(f) { }
};

 // A reference-counted reference to a Route.
struct SharedRoute {
    in::RCP<const Route, in::delete_Route> data;
    explicit SharedRoute (const Route* p) : data(p) { }
     // The empty Route will null-deref if you try to do anything but boolify
     // it.  When transformed into a reference will yield the empty AnyRef.
    constexpr SharedRoute () { }
     // Constructs a root Route from a Resource.
    explicit SharedRoute (ResourceRef) noexcept;
     // Constructs a root Route from an anonymous item.  as_iri() will return
     // "ayu-anonymous:", and reference_from_route will return this AnyRef.
    explicit SharedRoute (const AnyRef&) noexcept;
     // Append an attribute key or an element index to the Route.
    SharedRoute (SharedRoute parent, AnyString key) noexcept;
    SharedRoute (SharedRoute parent, u32 index) noexcept;

     // Check if this Route is empty.
    constexpr explicit operator bool () const { return !!data; }
     // Access Route interface
    const Route& operator* () const { return *data; }
    const Route* operator-> () const { return data.p; }
};

 // A non-owning reference to a Route.
struct RouteRef {
    const Route* data;
    constexpr RouteRef (const SharedRoute& p = {}) : data(p.data.p) { }
    explicit RouteRef (const Route* p) : data(p) { }

    constexpr explicit operator bool () const { return !!data; }
    const Route& operator* () const { return *data; }
    const Route* operator-> () const { return data; }
    operator SharedRoute () const { return SharedRoute(data); }
};

///// REFERENCE CONVERSION

 // Convert a Route to a AnyRef.  This will not have to do any scanning,
 // so it should be fairly quick.  Well, quicker than reference_to_route.
 // reference_to_route is in scan.h
AnyRef reference_from_route (RouteRef);

///// IRI CONVERSION

 // Gets an IRI corresponding to the given Route.  If the root is a resource,
 // the IRI up to the fragment will be the resource's name.  If the root is a
 // reference, the non-fragment part of the IRI will be "ayu-anonymous:".  A key
 // Route will have /key appended to the fragment, and an index Route will have
 // +index appended to the fragment.
IRI route_to_iri (RouteRef) noexcept;

 // Parses an IRI into a Route.  All of the IRI up to the fragment will
 // be used as the resource name for the root, and the fragment will be
 // processed as follows:
 //   - The empty fragment corresponds to the root
 //   - Appending /<string> will create a Route with an attr key
 //   - Appending +<number> will create a Route with an elem index
 //   - Literal / and + must be percent-encoded
 //   - At the begining of the fragment, "#foo" is shorthand for "#/foo+1".
 //     This is because a lot of documents are a collection of named typed
 //     items.
 // So
 //     route_from_iri("foo#/bar+3/qux")
 // is equivalent to
 //     Route(Route(Route(Route(Resource("foo")), "bar"), 3), "qux")
 // and calling reference_from_route on that is equivalent to
 //     Resource("foo")["bar"][3]["qux"]
 //
 // Throws if a + is followed by something that isn't a positive integer, or if
 // the IRI is just plain invalid.
SharedRoute route_from_iri (const IRI& iri);

 // Go straight from an IRI to a reference.  If you're using the resource
 // system, you probably want to use the two-argument form of ayu::track
 // instead.
inline AnyRef reference_from_iri (const IRI& iri) {
    auto rt = route_from_iri(iri);
    return reference_from_route(rt);
}

constexpr ErrorCode e_RouteIRIInvalid = "ayu::e_RouteIRIInvalid";

///// CURRENT BASE MANAGEMENT

 // This API is publicly visible but it's probably not very useful to use
 // directly.

 // Similar to web documents, there's a concept of a base IRI.  Relative IRI
 // reference strings are read and written relative to this base IRI.  The
 // current base is set whenever a traversal operation happens.  If the
 // traversal operation is passed a route, the base route is the root of that
 // route.  If not, it's an anonymous reference route of whatever reference was
 // passed to the traversal operation.
struct CurrentBase;

 // Null if there is no current base.  This will never be null during a
 // traversal operation, and by extension during any to_tree or from_tree
 // function.  It is not guaranteed to be non-null during a computed_attr or
 // computed_elem function, or during any accessor functions, because those
 // could be called by using AnyRefs, not just during traversals.
inline const CurrentBase* current_base = null;

struct CurrentBase {
    SharedRoute route; // Always a Resource Route or Reference Route.
    const IRI& iri () const noexcept;

     // Creating a CurrentBase object will set the current base to the given
     // route's root (and its corresponding IRI) and destroying it will revert
     // the current base to what it was before.  You can have multiple
     // CurrentBase objects and they act like a stack.  They must be destroying
     // in reverse order of construction.
     //
     // You probably mostly do not want to set the CurrentBase yourself.
    CurrentBase (RouteRef rt) : route(rt->root()) {
        old = current_base;
        current_base = this;
    }
    CurrentBase (RouteRef rt, const AnyRef& item) :
        route(rt ? SharedRoute(rt->root()) : SharedRoute(item))
    {
        old = current_base;
        current_base = this;
    }
    ~CurrentBase () {
        expect(current_base == this);
        current_base = old;
        expect(route);
    }

    mutable IRI iri_; // Lazily generated
    const CurrentBase* old;
};

} // namespace ayu

#include "route.inline.h"
