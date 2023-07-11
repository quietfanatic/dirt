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

#include "internal/common-internal.h"

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
    explicit Location (Resource);
     // Constructs a root location from an anonymous item.  as_iri() will return
     // "anonymous-item:", and reference_from_location will return this
     // Reference.
    explicit Location (Reference);
     // Constructs a location based on another one with an added attribute key
     // or element index.
    Location (Location parent, AnyString key);
    Location (Location parent, usize index);

     // Returns null if this is not a resource root.
    const Resource* resource () const;
     // Returns null if this is not a reference root.
    const Reference* reference () const;
     // Returns null if this is a root.
    const Location* parent () const;
     // Returns null if this location is a root or has an index.
    const AnyString* key () const;
     // Returns null if this location is a root or has a key.
    const usize* index () const;

     // Walks down to the root Location (containing either a Resource or a
     // Reference) and returns it.
    Location root () const;
};

bool operator == (LocationRef a, LocationRef b) noexcept;

 // Convert a Location to a Reference.  This will not have to do any scanning,
 // so it should be fairly quick.  Well, quicker than reference_to_location.
 // reference_to_location is in scan.h
Reference reference_from_location (LocationRef);

 // Parse the given IRI reference relative to current_root_location().  An
 // empty fragment will also be stripped off.
IRI location_iri_from_relative_iri (Str);

 // Convert the given IRI to an IRI reference relative to
 // current_root_location().  If the given IRI is exactly
 // current_root_location(), this will return "#".
AnyString location_iri_to_relative_iri (const IRI&);

 // Gets an IRI corresponding to the given Location.
IRI location_to_iri (LocationRef);

 // Parses an IRI into a location.  All of the IRI up to the fragment will
 // be used as the resource name for the root, and the fragment will be
 // split on / and each segment used as either a key or index.  To force a
 // string of digits to be used as a key instead of an index, precede it
 // with ' (apostrophe).  To start a key with a literal ', start it
 // with two 's.  To put a literal / in a key, use %2F.
Location location_from_iri (const IRI& iri);

} // namespace ayu
