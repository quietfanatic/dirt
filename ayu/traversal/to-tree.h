// This module contains functions to serialize items into trees.
//
// Serialization functions cannot be used until main() starts.

#pragma once

#include "../common.h"
#include "../data/print.h"
#include "../data/tree.h"
#include "../reflection/reference.h"
#include "location.h"

namespace ayu {

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

 // While this object is alive, if an exception is thrown while serializing an
 // item (and that exception is described to AYU), then the exception will be
 // caught and reported inline in the serialized output.  It will be in a format
 // that is not valid to read back in, so only use it for debugging.
 // Internally, this is used when generating the .what() message for exceptions
 // TODO: replace with a flag
struct DiagnosticSerialization {
    [[nodiscard]] DiagnosticSerialization ();
    ~DiagnosticSerialization ();
};

 // Shortcuts
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

 // Called item_to_tree on an item that has no way of doing the to_tree
 // operation.  item_to_tree can also throw errors with the error codes in
 // serialize-compound.h.
constexpr ErrorCode e_ToTreeNotSupported = "ayu::e_ToTreeNotSupported";
 // Called item_to_tree on an item that only has a values() descriptor, but the
 // given tree did not match any of its values.
constexpr ErrorCode e_ToTreeValueNotFound = "ayu::e_ToTreeValueNotFound";

} // namespace ayu
