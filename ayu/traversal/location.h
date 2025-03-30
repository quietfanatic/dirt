// A Location is an intermediate step between an AnyRef and an IRI.  A valid
// Location can be easily converted to and from a valid IRI.  A Location can
// also be easily converted to a AnyRef, but converting a AnyRef to a
// Location may require scanning a lot of data.  The functions for doing these
// conversions are in scan.h.
//
// You shouldn't have to use this class directly, but I guess you can if you
// want to.
//
// Internally, a Location is a recursive object that is a symbolic
// representation of a AnyRef, explaining how to reach the referend from the
// root Resource by a chain of item_attr() and item_elem() calls. In ADT syntax,
//     data Location = RootLocation Resource
//                   | RefLocation AnyRef
//                   | KeyLocation Location AnyString
//                   | IndexLocation Location u32
//
// TODO: Provide functions to translate AnyRefs directly to and from IRIs
// somewhere.

#pragma once

#include "../../uni/lilac.h"
#include "../common.internal.h"
#include "../reflection/anyref.h"

namespace ayu {

enum class LocationForm : u8 {
    Resource,
    Reference,
    Key,
    Index,
};
using LF = LocationForm;

 // The abstract interface for Locations.
struct Location : in::RefCounted {
    LocationForm form;
     // Returns empty if this is not a resource root.
    ResourceRef resource () const noexcept;
     // Returns null if this is not a reference root.
    const AnyRef* reference () const noexcept;
     // Returns empty if this is a root.
    LocationRef parent () const noexcept;
     // Returns null if this location is a root or has an index.
    const AnyString* key () const noexcept;
     // Returns null if this location is a root or has a key.
    const u32* index () const noexcept;

     // Walks down to the root Location (containing either a Resource or a
     // AnyRef) and returns it.
    LocationRef root () const noexcept;

    static void* operator new (usize s) {
        return lilac::allocate_fixed_size(s);
    }
    static void operator delete (void* p, usize s) {
        lilac::deallocate_fixed_size(p, s);
    }
    protected:
    Location (LocationForm f) : form(f) { }
};

struct SharedLocation {
    in::RCP<const Location, in::delete_Location> data;
    explicit SharedLocation (const Location* p) : data(p) { }
     // The empty location cannot be transformed into a reference and will
     // null-deref if you try to do anything but boolify it.
    constexpr SharedLocation () { }
     // Constructs a root location from a Resource.  TODO: Change to
     // MoveRef<SharedResource>
    explicit SharedLocation (ResourceRef) noexcept;
     // Constructs a root location from an anonymous item.  as_iri() will return
     // "anonymous-item:", and reference_from_location will return this
     // AnyRef.
    explicit SharedLocation (const AnyRef&) noexcept;
     // Constructs a location based on another one with an added attribute key
     // or element index.
    SharedLocation (MoveRef<SharedLocation> parent, MoveRef<AnyString> key) noexcept;
    SharedLocation (MoveRef<SharedLocation> parent, u32 index) noexcept;

    constexpr explicit operator bool () const { return !!data; }
    const Location& operator* () const { return *data; }
    const Location* operator-> () const { return data.p; }
};

struct LocationRef {
    const Location* data;
    constexpr LocationRef (const SharedLocation& p = {}) : data(p.data.p) { }
    explicit LocationRef (const Location* p) : data(p) { }

    constexpr explicit operator bool () const { return !!data; }
    const Location& operator* () const { return *data; }
    const Location* operator-> () const { return data; }
    operator SharedLocation () const { return SharedLocation(data); }
};

///// REFERENCE CONVERSION

 // Convert a Location to a AnyRef.  This will not have to do any scanning,
 // so it should be fairly quick.  Well, quicker than reference_to_location.
 // reference_to_location is in scan.h
AnyRef reference_from_location (LocationRef);

///// IRI CONVERSION

 // Gets an IRI corresponding to the given Location.  If the root is a resource,
 // the IRI up to the fragment will be the resource's name.  If the root is a
 // reference, the non-fragment part of the IRI will be "ayu-anonymous:".
 // A key location will have /key appended to the fragment, and an index
 // location will have +index appended to the fragment.
IRI location_to_iri (LocationRef) noexcept;

 // Parses an IRI into a location.  All of the IRI up to the fragment will
 // be used as the resource name for the root, and the fragment will be
 // processed as follows:
 //   - The empty fragment corresponds to the root
 //   - Appending /<string> will create a location with an attr key
 //   - Appending +<number> will create a location with an elem index
 //   - Literal / and + must be percent-encoded
 // So
 //     location_from_iri("foo#/bar+3/qux")
 // is equivalent to
 //     Location(Location(Location(Location(Resource("foo")), "bar"), 3), "qux")
 // and calling reference_from_location on that is equivalent to
 //     Resource("foo")["bar"][3]["qux"]
 //
 // Throws if there is anything between the # and the first / or +, or if a + is
 // followed by something that isn't a positive integer, or if the IRI is just
 // plain invalid.
SharedLocation location_from_iri (const IRI& iri);

 // For convenience I guess
inline AnyRef reference_from_iri (const IRI& iri) {
    auto loc = location_from_iri(iri);
    return reference_from_location(loc);
}

constexpr ErrorCode e_LocationIRIInvalid = "ayu::e_LocationIRIInvalid";

///// BASE MANAGEMENT

 // Get the current base location.  Always a Resource or AnyRef Location.
LocationRef current_base_location () noexcept;
 // The IRI corresponding to current_base_location().  Will always have an
 // existing but empty #fragment.  When serializing IRIs with AYU, they will be
 // read and written as relative IRI reference strings, relative to this IRI.
IRI current_base_iri () noexcept;
 // Temporarily set loc->root() as the current base location.  This is called in
 // item_to_tree and item_from_tree.
struct PushBaseLocation {
    SharedLocation old_base_location;
    [[nodiscard]] PushBaseLocation (LocationRef loc) noexcept;
    ~PushBaseLocation ();
};

///// MISC

 // Add a traversal location to the current exception if it doesn't already
 // have one.
[[noreturn, gnu::cold]] void rethrow_with_travloc (LocationRef loc);

} // namespace ayu

#include "location.inline.h"
