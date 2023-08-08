// A Location is an intermediate step between a Reference and an IRI.  A valid
// Location can be easily converted to and from a valid IRI.  A Location can
// also be easily converted to a Reference, but converting a Reference to a
// Location may require scanning a lot of data.  The functions for doing these
// conversions are in serialize.h.
//
// You shouldn't have to use this class directly, but I guess you can if you
// want to.
//
// Internally, a Location is a recursive object that is a symbolic
// representation of a Reference, explaining how to reach the referend from the
// root Resource by a chain of item_attr() and item_elem() calls. In ADT syntax,
//     data Location = RootLocation Resource
//                   | ReferenceLocation Reference
//                   | KeyLocation Location AnyString
//                   | IndexLocation Location usize
//
// TODO: Provide functions to translate References directly to and from IRIs
// somewhere.

#pragma once

#include "../common.internal.h"

namespace ayu {

 // Locations are a reference-counted pointer, so are cheap to copy.  Locations are
 // immutable once created.
struct Location {
    in::RCP<in::LocationData, in::delete_LocationData> data;
    constexpr explicit Location (in::LocationData* p = null) : data(p) { }
     // The empty location is treated as the location of an anonymous item, and
     // can't be transformed into a reference.
    explicit operator bool () const { return !!data; }
     // Constructs a root location from a Resource.
    explicit Location (Resource) noexcept;
     // Constructs a root location from an anonymous item.  as_iri() will return
     // "anonymous-item:", and reference_from_location will return this
     // Reference.
    explicit Location (const Reference&) noexcept;
     // Constructs a location based on another one with an added attribute key
     // or element index.
    Location (MoveRef<Location> parent, MoveRef<AnyString> key) noexcept;
    Location (MoveRef<Location> parent, usize index) noexcept;

     // Returns null if this is not a resource root.
    const Resource* resource () const noexcept;
     // Returns null if this is not a reference root.
    const Reference* reference () const noexcept;
     // Returns null if this is a root.
    const Location* parent () const noexcept;
     // Returns null if this location is a root or has an index.
    const AnyString* key () const noexcept;
     // Returns null if this location is a root or has a key.
    const uint32* index () const noexcept;

     // Walks down to the root Location (containing either a Resource or a
     // Reference) and returns it.
    Location root () const noexcept;
};

///// REFERENCE CONVERSION

 // Convert a Location to a Reference.  This will not have to do any scanning,
 // so it should be fairly quick.  Well, quicker than reference_to_location.
 // reference_to_location is in scan.h
Reference reference_from_location (LocationRef);

///// IRI CONVERSION

 // Gets an IRI corresponding to the given Location.  If the root is a Resource,
 // the IRI up to the fragment will be the resource's name.  If the root is a
 // Reference, the non-fragment part of the IRI will be "ayu-anonymous:".
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
Location location_from_iri (const IRI& iri);

constexpr ErrorCode e_LocationIRIInvalid = "ayu::e_LocationIRIInvalid";

///// BASE MANAGEMENT

 // Get the current base location.  Always a Resource or Reference Location.
Location current_base_location () noexcept;
 // The IRI corresponding to current_base_location().
 // When serializing IRIS with AYU, they will be read and written as relative
 // IRI reference strings, relative to this IRI.
IRI current_base_iri () noexcept;
 // Temporarily set loc.root() as the current base location.  This is called in
 // item_to_tree and item_from_tree.
struct PushBaseLocation {
    Location old_base_location;
    [[nodiscard]] PushBaseLocation (Location loc) noexcept;
    ~PushBaseLocation ();
};

} // namespace ayu

#include "location.inline.h"
