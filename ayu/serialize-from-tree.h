// This module contains the meat of the serialization functionality of this
// library, implementing algorithms to transform objects to and from trees,
// based on the information in their descriptions.
//
// Serialization functions cannot be used until main() starts.

#pragma once

#include "common.h"
#include "location.h"
#include "parse.h"
#include "reference.h"
#include "tree.h"

namespace ayu {

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

 // Shortcuts
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

} // ayu
