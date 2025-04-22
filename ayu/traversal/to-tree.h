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

 // Flags to change the behavior of item_to_tree and related functions.
enum class ToTreeOptions {
     // If an exception is thrown while serializing an item, then the exception
     // will be caught and reported inline in the serialized output.  For
     // *_to_tree, it will be wrapped in a Tree of form TreeForm::Error.  For
     // *_to_string, it will be written as "!(exception's .what() value)".  This
     // not valid to read back in with *_from_string, so you should only use
     // this option for diagnostics or human-consumption strings.
     //
     // Using this option makes item_to_tree effectively noexcept.
    EmbedErrors = 1,
};
DECLARE_ENUM_BITWISE_OPERATORS(ToTreeOptions);
using TTO = ToTreeOptions;

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
Tree item_to_tree (const AnyRef&, RouteRef rt = {}, ToTreeOptions opts = {});
 // Slight optimization for pointers (the usual case)
template <class T>
Tree item_to_tree (T* item, RouteRef rt = {}, ToTreeOptions opts = {});

///// Shortcuts

UniqueString item_to_string (
    const AnyRef& item, PrintOptions popts = {},
    RouteRef rt = {}, ToTreeOptions ttopts = {}
);
template <class T>
UniqueString item_to_string (
    T* item, PrintOptions popts = {},
    RouteRef rt = {}, ToTreeOptions ttopts = {}
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
Tree item_to_tree (T* item, RouteRef rt, ToTreeOptions opts) {
    Tree r;
    in::FakeRef fake {.ref = item};
    return item_to_tree(fake.ref, rt, opts);
}

template <class T>
UniqueString item_to_string (
    T* item, PrintOptions popts, RouteRef rt, ToTreeOptions ttopts
) {
    in::FakeRef fake {.ref = item};
    return item_to_string(fake.ref, popts, rt, ttopts);
}

} // ayu
