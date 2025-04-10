// This module contains functions to serialize items into trees.
//
// Serialization functions cannot be used until main() starts.

#pragma once

#include "../common.h"
#include "../data/print.h"
#include "../data/tree.h"
#include "../reflection/anyref.h"
#include "route.h"

namespace ayu {

 // Convert an item to a tree.  The optional route should match the reference's
 // route if provided.  One of the below AYU_DESCRIBE descriptors will be used
 // for this, with earlier ones preferred over later ones:
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
Tree item_to_tree (const AnyRef&, RouteRef rt = {});
 // Slight optimization for pointers (the usual case)
template <class T>
Tree item_to_tree (T* item, RouteRef rt = {});

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

///// Shortcuts

UniqueString item_to_string (
    const AnyRef& item, PrintOptions opts = {},
    RouteRef rt = {}
);
template <class T>
UniqueString item_to_string (
    T* item, PrintOptions opts = {}, RouteRef rt = {}
);

///// Error codes

 // Called item_to_tree on an item that has no way of doing the to_tree
 // operation.  item_to_tree can also throw errors with the error codes in
 // serialize-compound.h.
constexpr ErrorCode e_ToTreeNotSupported = "ayu::e_ToTreeNotSupported";
 // Called item_to_tree on an item that only has a values() descriptor, but the
 // given tree did not match any of its values.
constexpr ErrorCode e_ToTreeValueNotFound = "ayu::e_ToTreeValueNotFound";

///// Inlines

template <class T>
Tree item_to_tree (T* item, RouteRef rt) {
    Tree r;
    in::FakeRef fake {.ref = item};
    return item_to_tree(fake.ref, rt);
}

template <class T>
UniqueString item_to_string (T* item, PrintOptions opts, RouteRef rt) {
    in::FakeRef fake {.ref = item};
    return item_to_string(fake.ref, opts, rt);
}

} // ayu
