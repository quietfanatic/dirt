#pragma once

#include "../common.h"
#include "../location.h"
#include "accessors-private.h"
#include "descriptors-private.h"

namespace ayu::in {

 // This tracks the decisions that were made during a serialization operation.
 // It has two purposes:
 //   1. Allow creating a Reference to the current item in case the current item
 //     is not addressable, without having to start over from the very beginning
 //     or duplicate work.  This is mainly to support swizzle and init ops.
 //   2. Track the current location without any heap allocations, but allow
 //     getting an actual heap-allocated Location to the current item if needed
 //     for error reporting.
enum TraversalOp : uint8 {
    START,
    DELEGATE,
    ATTR,
    ATTR_FUNC,
    ELEM,
    ELEM_FUNC,
};
struct Traversal;
inline const Traversal* current_traversal = null;
struct Traversal {
    const Traversal* parent;
    Mu* address;
    const DescriptionPrivate* desc;
     // Type can keep track of readonly, but DescriptionPrivate* can't, so keep
     // track of it here.
    bool readonly;
     // If this item has a stable address, then trav_reference() can use the
     // address directly instead of having to chain from parent.
    bool addressable;
     // Set if this item has pass_through_addressable AND parent->addressable is
     // true.
    bool children_addressable;
     // Only traverse addressable items.  If an unaddressable and
     // non-pass-through item is encountered, the traversal's callback will not
     // be called.
    bool only_addressable;
    TraversalOp op;
    union {
         // START
        const Reference* reference;
         // DELEGATE, ATTR, ELEM
        const Accessor* acr;
         // ATTR_FUNC
        Reference(* attr_func )(Mu&, AnyString);
         // ELEM_FUNC
        Reference(* elem_func )(Mu&, usize);
    };
    union {
         // START
        LocationRef location;
         // ATTR, ATTR_FUNC
         // Can't include AnyString directly because it's too non-trivial.
         // TODO: CRef<AnyString> then?
        const AnyString* key;
         // ELEM, ELEM_FUNC
        usize index;
    };
    Traversal() : parent(current_traversal) {
        current_traversal = this;
    }
    ~Traversal() {
        expect(current_traversal == this);
        current_traversal = parent;
    }
};

using TravCallbackRef = CallbackRef<void(const Traversal&)>;

 // The trav_ functions are the bones of the serialization and scanning modules,
 // and are worth heavily optimizing.
template <class CB>
static inline void trav_start (
    const Reference& ref, LocationRef loc, bool only_addressable,
    AccessMode mode, CB cb
) {
    expect(ref);
    Traversal trav;
    trav.only_addressable = only_addressable;
    trav.op = START;
    trav.reference = &ref;
    trav.location = loc;
    trav.address = ref.address();
     // I experimented with several ways of branching, and this one works out
     // the best.  ref.address(), ref.type(), and ref.readonly() all branch on
     // the existence of ref.acr, so if we bundle them up this way, the
     // compiler can make it so the happy path where ref.acr == null only has
     // one branch.  (For some reason putting ref.type() and ref.readonly()
     // before ref.address() doesn't work as well, so I put them after).
    if (trav.address) {
        trav.readonly = ref.readonly();
        trav.desc = DescriptionPrivate::get(ref.type());
        trav.addressable = true;
        trav.children_addressable = true;
        cb(trav);
    }
    else {
        trav.readonly = ref.readonly();
        trav.desc = DescriptionPrivate::get(ref.type());
        trav.addressable = false;
        trav.children_addressable =
            ref.acr->accessor_flags & ACR_PASS_THROUGH_ADDRESSABLE;
        if (!trav.only_addressable || trav.children_addressable) {
            ref.access(mode, [&](Mu& v){
                trav.address = &v;
                cb(trav);
            });
        }
    }
}

template <class CB>
static inline void trav_acr (
    Traversal& trav, const Traversal& parent, const Accessor* acr,
    AccessMode mode, CB cb
) {
    trav.readonly = parent.readonly || acr->accessor_flags & ACR_READONLY;
    trav.only_addressable = parent.only_addressable;
    trav.acr = acr;
    trav.desc = DescriptionPrivate::get(acr->type(parent.address));
    trav.address = acr->address(*parent.address);
    if (trav.address) {
        trav.addressable = parent.children_addressable;
        trav.children_addressable = parent.children_addressable;
        cb(trav);
    }
    else {
        trav.addressable = false;
        trav.children_addressable =
            acr->accessor_flags & ACR_PASS_THROUGH_ADDRESSABLE;
        if (!trav.only_addressable || trav.children_addressable) {
            acr->access(mode, *parent.address, [&](Mu& v){
                trav.address = &v;
                cb(trav);
            });
        }
    }
}

 // Add another template parameter to encourage more inlining (CB is always
 // TravCallbackRef here)
template <class CB, TraversalOp op>
static inline void trav_ref (
    Traversal& trav, const Traversal& parent, const Reference& ref,
    AccessMode mode, CB cb
) {
    trav.only_addressable = parent.only_addressable;
    trav.address = ref.address();
    if (trav.address) {
        trav.desc = DescriptionPrivate::get(ref.type());
        trav.readonly = parent.readonly || ref.readonly();
        trav.addressable = parent.children_addressable;
        trav.children_addressable = parent.children_addressable;
        cb(trav);
    }
    else {
        trav.desc = DescriptionPrivate::get(ref.type());
        trav.readonly = parent.readonly || ref.readonly();
        trav.addressable = false;
        trav.children_addressable =
            ref.acr->accessor_flags & ACR_PASS_THROUGH_ADDRESSABLE;
        if (!trav.only_addressable || trav.children_addressable) {
            ref.access(mode, [&](Mu& v){
                trav.address = &v;
                cb(trav);
            });
        }
    }
}

template <class CB>
static inline void trav_delegate (
    const Traversal& parent, const Accessor* acr,
    AccessMode mode, CB cb
) {
    expect(&parent == current_traversal);
    Traversal trav;
    trav.op = DELEGATE;
    trav_acr(trav, parent, acr, mode, cb);
}

 // key is a reference instead of a pointer so that a temporary can be passed
 // in.  The pointer will be released when this function returns, so no worry
 // about a dangling pointer to a temporary.
template <class CB>
static inline void trav_attr (
    const Traversal& parent, const Accessor* acr, const AnyString& key,
    AccessMode mode, CB cb
) {
    expect(&parent == current_traversal);
    Traversal trav;
    trav.op = ATTR;
    trav.key = &key;
    trav_acr(trav, parent, acr, mode, cb);
}

template <class CB>
static inline void trav_attr_func (
    const Traversal& parent, const Reference& ref,
    Reference(* func )(Mu&, AnyString), const AnyString& key,
    AccessMode mode, CB cb
) {
    expect(&parent == current_traversal);
    Traversal trav;
    trav.op = ATTR_FUNC;
    trav.attr_func = func;
    trav.key = &key;
    trav_ref<CB, ATTR_FUNC>(trav, parent, ref, mode, cb);
}

template <class CB>
static inline void trav_elem (
    const Traversal& parent, const Accessor* acr, usize index,
    AccessMode mode, CB cb
) {
    expect(&parent == current_traversal);
    Traversal trav;
    trav.op = ELEM;
    trav.index = index;
    trav_acr(trav, parent, acr, mode, cb);
}

template <class CB>
static inline void trav_elem_func (
    const Traversal& parent, const Reference& ref,
    Reference(* func )(Mu&, usize), usize index,
    AccessMode mode, CB cb
) {
    expect(&parent == current_traversal);
    Traversal trav;
    trav.op = ELEM_FUNC;
    trav.elem_func = func;
    trav.index = index;
    trav_ref<CB, ELEM_FUNC>(trav, parent, ref, mode, cb);
}

inline Reference trav_reference (const Traversal& trav) noexcept {
    if (trav.addressable) {
        return Pointer(
            Type(trav.desc, trav.readonly),
            trav.address
        );
    }
    else if (trav.op == START) {
        return *trav.reference;
    }
    else {
        Reference parent = trav_reference(*trav.parent);
        switch (trav.op) {
            case DELEGATE: case ATTR: case ELEM:
                return parent.chain(trav.acr);
            case ATTR_FUNC:
                return parent.chain_attr_func(trav.attr_func, *trav.key);
            case ELEM_FUNC:
                return parent.chain_elem_func(trav.elem_func, trav.index);
            default: never();
        }
    }
}

inline Location trav_location (const Traversal& trav) noexcept {
    if (trav.op == START) {
        if (*trav.location) return trav.location;
         // This * took a half a day of debugging to add. :(
        else return Location(*trav.reference);
    }
    else {
        Location parent = trav_location(*trav.parent);
        switch (trav.op) {
            case DELEGATE: return parent;
            case ATTR: case ATTR_FUNC:
                return Location(move(parent), *trav.key);
            case ELEM: case ELEM_FUNC:
                return Location(move(parent), trav.index);
            default: never();
        }
    }
}

inline const Traversal& find_trav_start (const Traversal& trav) {
    auto tr = &trav;
    while (tr->op != START) tr = tr->parent;
    return *tr;
}

} // namespace ayu::in
