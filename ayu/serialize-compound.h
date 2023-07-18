 // This module contains functions to manipulate compound objects using the
 // traversal system.

#pragma once

#include "common.h"
#include "location.h"

namespace ayu {

///// OPERATIONS FOR OBJECT-LIKE TYPES
 // These use either the attrs() descriptor or the keys() and attr_func()
 // descriptors.  If neither are available, they use the delegate() descriptor,
 // and if that isn't available, they throw a NoAttrs exception.

 // Get a list of the keys in a object-like item.
AnyArray<AnyString> item_get_keys (
    const Reference&, LocationRef loc = Location()
);
 // Set the keys in an object-like item.  This may clear the entire contents
 // of the item.  If the item only accepts certain attribute keys, this may
 // throw MissingAttr or UnwantedAttr.
void item_set_keys (
    const Reference&, AnyArray<AnyString>,
    LocationRef loc = Location()
);
 // Get an attribute of an object-like item by its key, or empty Reference if
 // the attribute doesn't exist.
Reference item_maybe_attr (
    const Reference&, AnyString, LocationRef loc = Location());
 // Throws AttrNotFound if the attribute doesn't exist.  Guaranteed not to
 // return an empty or null Reference.
Reference item_attr (const Reference&, AnyString, LocationRef loc = Location());

///// OPERATIONS FOR ARRAY-LIKE TYPES
 // These use either the elems() descriptor or the keys() and elem_func()
 // descriptors.  If neither are available, they use the delegate() descriptor,
 // and if that isn't available, they throw a NoElems exception.

 // Get the length of an array-like item.
usize item_get_length (const Reference&, LocationRef loc = Location());
 // Set the length of an array-like item.  This may clear some or all of the
 // contents of the item.  If the item only allows certain lengths, this may
 // throw WrongLength.
void item_set_length (
    const Reference&, usize, LocationRef loc = Location()
);
 // Get an element of an array-like item by its index.  Returns an empty
 // Reference if the element doesn't exist.
Reference item_maybe_elem (
    const Reference&, usize, LocationRef loc = Location()
);
 // Throws ElemNotFound if the element doesn't exist.  Guaranteed not to return
 // an empty or null Reference.
Reference item_elem (
    const Reference&, usize, LocationRef loc = Location()
);

} // namespace ayu
