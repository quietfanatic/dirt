#pragma once
#include "../serialize.h"

#include "../reference.h"
#include "../resource.h"
#include "descriptors-private.h"
#include "location-private.h"
#include "traversal-private.h"

namespace ayu::in {

///// TO_TREE
Tree ser_to_tree (const Traversal&);

///// FROM_TREE
struct SwizzleOp {
    using FP = void(*)(Mu&, const Tree&);
    FP f;
    Reference item;
     // This can't be TreeRef because the referenced Tree could go away after a
     // nested from_tree is called with DELAY_SWIZZLE
    Tree tree;
    Location loc;
};
struct InitOp {
    using FP = void(*)(Mu&);
    FP f;
    Reference item;
    Location loc;
};
struct IFTContext {
    static IFTContext* current;
    IFTContext* previous;
    IFTContext () : previous(current) {
        current = this;
    }
    ~IFTContext () {
        expect(current == this);
        current = previous;
    }

    UniqueArray<SwizzleOp> swizzle_ops;
    UniqueArray<InitOp> init_ops;
    void do_swizzles ();
    void do_inits ();
};
void ser_from_tree (const Traversal&, TreeRef);

///// ATTR OPERATIONS
 // Implement get_keys by adding keys to an array of AnyStrings
void ser_collect_key (UniqueArray<AnyString>&, AnyString&&);
void ser_collect_keys (const Traversal&, UniqueArray<AnyString>&);

 // Implement set_keys by removing keys from an array
bool ser_claim_key (UniqueArray<AnyString>&, Str);
void ser_claim_keys (const Traversal&, UniqueArray<AnyString>&, bool optional);
void ser_set_keys (const Traversal&, UniqueArray<AnyString>&&);

 // If the attr isn't found, returns false and doesn't call the callback
bool ser_maybe_attr (const Traversal&, const AnyString&, AccessMode, TravCallbackRef);
 // Throws if the attr isn't found
void ser_attr (const Traversal&, const AnyString&, AccessMode, TravCallbackRef);

 ///// Elem operations
usize ser_get_length (const Traversal&);
 // Implement set_length by counting up used length
void ser_claim_length (const Traversal&, usize& claimed, usize len);
void ser_set_length (const Traversal&, usize);

 // If elem is out of range, returns false and doesn't call the callback
bool ser_maybe_elem (const Traversal&, usize, AccessMode, TravCallbackRef);
 // Throws if elem is out of bounds
void ser_elem (const Traversal&, usize, AccessMode, TravCallbackRef);

} // namespace ayu::in
