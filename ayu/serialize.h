// This module contains the meat of the serialization functionality of this
// library, implementing algorithms to transform objects to and from trees,
// based on the information in their descriptions.
//
// Serialization functions cannot be used until main() starts.

#pragma once

#include "common.h"
#include "location.h"
#include "parse.h"
#include "print.h"
#include "tree.h"
#include "type.h"

namespace ayu {

////// MAIN OPERATIONS
 // Convert an item to a tree.  The optional location should match the
 // reference's location if provided.  One of the below AYU_DESCRIBE descriptors
 // will be used for this, with earlier ones preferred over later ones:
 //   1. to_tree()
 //   2. values() if any of them match the item
 //   3. whichever of these was declared first in the description:
 //       - attrs()
 //       - elems()
 //       - keys() and attr_func()
 //       - length() and elem_func()
 //   4. delegate()
 // If none of the above are applicable, a CannotToTree exception will be
 // thrown.
Tree item_to_tree (
    const Reference&, LocationRef loc = Location()
);

 // Flags to change the behavior of item_from_tree.
using ItemFromTreeFlags = uint32;
enum : ItemFromTreeFlags {
     // If calling item_from_tree recursively, schedule swizzle and init
     // operations for after the outer call does its swizzle and init
     // operations respectively.  This will allow items to cyclically reference
     // one another, but can only be used if
     //   A: the provided reference will still be valid later on (e.g it's not
     //      the address of a stack temporary that's about to be moved into a
     //      container), and
     //   B: the item's treatment will not change based on its value.  For
     //      instance, this is not usable on the elements of a
     //      std::unordered_set, because the position of a set element depends
     //      on its value, and updating it in place without notifying the
     //      unordered_set would corrupt the unordered_set.
     // item_from_tree cannot check that these conditions are true, so if you
     // use this flag when they are not true, you will likely corrupt memory.
     //
     // For non-recursive item_from_tree calls, this flag has no effect.
    DELAY_SWIZZLE = 1,
};
 // Write to an item from a tree.  If an exception is thrown, the item may be
 // left in an incomplete state, so if you're worried about that, construct a
 // fresh item, call item_from_tree on that, and then move it onto the original
 // item (this is what ayu::reload() on resources does).  One of the following
 // AYU_DESCRIBE descriptors will be used for deserialization, with earlier ones
 // prioritized over later ones:
 //   1. from_tree()
 //   2. values(), if any of them match the given Tree
 //   3. whichever of these is highest in the description:
 //       - attrs()
 //       - elems()
 //       - keys() and attr_func()
 //       - length() and elem_func()
 //   4. delegate()
 // If none of those descriptors are applicable, a CannotFromTree exception will
 // be thrown.
void item_from_tree (
    const Reference&, TreeRef, LocationRef loc = Location(),
    ItemFromTreeFlags flags = 0
);

///// MAIN OPERATION SHORTCUTS
inline UniqueString item_to_string (
    const Reference& item, PrintOptions opts = 0,
    LocationRef loc = Location()
) {
    return tree_to_string(item_to_tree(item, loc), opts);
}
inline void item_to_file (
    const Reference& item, AnyString filename,
    PrintOptions opts = 0, LocationRef loc = Location()
) {
    return tree_to_file(item_to_tree(item, loc), move(filename), opts);
}
 // item_from_string and item_from_file do not currently allow passing flags
inline void item_from_string (
    const Reference& item, Str src, LocationRef loc = Location()
) {
    return item_from_tree(item, tree_from_string(src), loc);
}
inline void item_from_file (
    const Reference& item, AnyString filename, LocationRef loc = Location()
) {
    return item_from_tree(item, tree_from_file(move(filename)), loc);
}

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

///// MISC

 // If a serialization operation is active, get the Location of an item currently
 // being processed.
Location current_location ();

///// DIAGNOSTICS HELP

 // While this object is alive, if an exception is thrown while serializing an
 // item (and that exception is described to AYU), then the exception will be
 // caught and reported inline in the serialized output.  It will be in a format
 // that is not valid to read back in, so only use it for debugging.
 // Internally, this is used when generating the .what() message for exceptions
struct DiagnosticSerialization {
    DiagnosticSerialization ();
    ~DiagnosticSerialization ();
};

 // Generic serialization error
struct SerError : Error {
    Location location;
    Type type;
    SerError (Location l, Type t) : location(move(l)), type(t) { }
};
 // Tried to call to_tree on a type that doesn't support to_tree
struct CannotToTree : SerError { using SerError::SerError; };
 // Tried to call from_tree on a type that doesn't support from_tree
struct CannotFromTree : SerError { using SerError::SerError; };
 // Tried to deserialize an item from a tree, but the item didn't accept
 // the tree's form.
struct InvalidForm : SerError {
    Tree tree;
    InvalidForm (Location l, Type ty, Tree tr) :
        SerError(move(l), ty), tree(tr)
    { }
};
 // Tried to serialize an item using a values() descriptor, but no value()
 // entry was found for the item's current value.
struct NoNameForValue : SerError { using SerError::SerError; };
 // Tried to deserialize an item using a values() descriptor, but no value()
 // entry was found that matched the provided name.
struct NoValueForName : SerError {
    Tree name;
    NoValueForName (Location l, Type t, Tree n) :
        SerError(move(l), t), name(n)
    { }
};
 // Tried to deserialize an item from an object tree, but the tree lacks an
 // attribute that the item requires.
struct MissingAttr : SerError {
    AnyString key;
    MissingAttr (Location l, Type t, AnyString k) :
        SerError(move(l), t), key(move(k))
    { }
};
 // Tried to deserialize an item from an object tree, but the item rejected
 // one of the attributes in the tree.
struct UnwantedAttr : SerError {
    AnyString key;
    UnwantedAttr (Location l, Type t, AnyString k) :
        SerError(move(l), t), key(move(k))
    { }
};
 // Tried to deserialize an item from an array tree, but the array has too
 // few or too many elements for the item.
struct WrongLength : SerError {
    usize min;
    usize max;
    usize got;
    WrongLength (Location l, Type t, usize mi, usize ma, usize g) :
        SerError(move(l), t), min(mi), max(ma), got(g)
    { }
};
 // Tried to treat an item like it has attributes, but it does not support
 // behaving like an object.
struct NoAttrs : SerError { using SerError::SerError; };
 // Tried to treat an item like it has elements, but it does not support
 // behaving like an array.
struct NoElems : SerError { using SerError::SerError; };
 // Tried to get an attribute from an item, but it doesn't have an attribute
 // with the given key.
struct AttrNotFound : SerError {
    AnyString key;
    AttrNotFound (Location l, Type t, AnyString k) :
        SerError(move(l), t), key(move(k))
    { }
};
 // Tried to get an element from an item, but it doesn't have an element
 // with the given index (the index is out of bounds).
struct ElemNotFound : SerError {
    usize index;
    ElemNotFound (Location l, Type t, usize i) :
        SerError(move(l), t), index(i)
    { }
};
 // The accessor given to a keys() descriptor did not serialize to an array
 // of strings.
struct InvalidKeysType : SerError {
    Type keys_type;
    InvalidKeysType (Location l, Type t, Type k) :
        SerError(move(l), t), keys_type(k)
    { }
};

} // namespace ayu
