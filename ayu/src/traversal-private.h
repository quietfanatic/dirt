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

    static const Traversal* current_start;

    template <class CB>
    static void start (
        const Reference& ref, LocationRef loc, bool only_addressable,
        AccessMode mode, CB cb
    ) {
        expect(ref);
        Traversal child;
        auto old_start = current_start;
        try {
            current_start = &child;
            child.only_addressable = only_addressable;
            child.op = START;
            child.reference = &ref;
            child.location = loc;
            child.address = ref.address();
             // I experimented with several ways of branching, and this one works out
             // the best.  ref.address(), ref.type(), and ref.readonly() all branch on
             // the existence of ref.acr, so if we bundle them up this way, the
             // compiler can make it so the happy path where ref.acr == null only has
             // one branch.  (For some reason putting ref.type() and ref.readonly()
             // before ref.address() doesn't work as well, so I put them after).
            if (child.address) {
                child.readonly = ref.readonly();
                child.desc = DescriptionPrivate::get(ref.type());
                child.addressable = true;
                child.children_addressable = true;
                cb(child);
            }
            else {
                child.readonly = ref.readonly();
                child.desc = DescriptionPrivate::get(ref.type());
                child.addressable = false;
                child.children_addressable =
                    ref.acr->accessor_flags & ACR_PASS_THROUGH_ADDRESSABLE;
                if (!child.only_addressable || child.children_addressable) {
                    ref.access(mode, [&](Mu& v){
                        child.address = &v;
                        cb(child);
                    });
                }
            }
            current_start = old_start;
        }
        catch (...) { current_start = old_start; throw; }
    }

    template <class CB>
    void follow_acr (
        Traversal& child, const Accessor* acr, AccessMode mode, CB cb
    ) const {
        child.parent = this;
        child.readonly = readonly || acr->accessor_flags & ACR_READONLY;
        child.only_addressable = only_addressable;
        child.acr = acr;
        child.desc = DescriptionPrivate::get(acr->type(address));
        child.address = acr->address(*address);
        if (child.address) {
            child.addressable = children_addressable;
            child.children_addressable = children_addressable;
            cb(child);
        }
        else {
            child.addressable = false;
            child.children_addressable =
                acr->accessor_flags & ACR_PASS_THROUGH_ADDRESSABLE;
            if (!child.only_addressable || child.children_addressable) {
                acr->access(mode, *address, [&](Mu& v){
                    child.address = &v;
                    cb(child);
                });
            }
        }
    }

    template <class CB>
    void follow_ref (
        Traversal& child, const Reference& ref, AccessMode mode, CB cb
    ) const {
        child.parent = this;
        child.only_addressable = only_addressable;
        child.address = ref.address();
        if (child.address) {
            child.desc = DescriptionPrivate::get(ref.type());
            child.readonly = readonly || ref.readonly();
            child.addressable = children_addressable;
            child.children_addressable = children_addressable;
            cb(child);
        }
        else {
            child.desc = DescriptionPrivate::get(ref.type());
            child.readonly = readonly || ref.readonly();
            child.addressable = false;
            child.children_addressable =
                ref.acr->accessor_flags & ACR_PASS_THROUGH_ADDRESSABLE;
            if (!child.only_addressable || child.children_addressable) {
                ref.access(mode, [&](Mu& v){
                    child.address = &v;
                    cb(child);
                });
            }
        }
    }

    template <class CB>
    void follow_delegate (
        const Accessor* acr, AccessMode mode, CB cb
    ) const {
        Traversal child;
        child.op = DELEGATE;
        follow_acr(child, acr, mode, cb);
    }

     // key is a reference instead of a pointer so that a temporary can be
     // passed in.  The pointer will be released when this function returns, so
     // no worry about a dangling pointer to a temporary.
    template <class CB>
    void follow_attr (
        const Accessor* acr, const AnyString& key, AccessMode mode, CB cb
    ) const {
        Traversal child;
        child.op = ATTR;
        child.key = &key;
        follow_acr(child, acr, mode, cb);
    }

    template <class CB>
    void follow_attr_func (
        const Reference& ref, Reference(* func )(Mu&, AnyString),
        const AnyString& key, AccessMode mode, CB cb
    ) const {
        Traversal child;
        child.op = ATTR_FUNC;
        child.attr_func = func;
        child.key = &key;
        follow_ref(child, ref, mode, cb);
    }

    template <class CB>
    void follow_elem (
        const Accessor* acr, usize index, AccessMode mode, CB cb
    ) const {
        Traversal child;
        child.op = ELEM;
        child.index = index;
        follow_acr(child, acr, mode, cb);
    }

    template <class CB>
    void follow_elem_func (
        const Reference& ref, Reference(* func )(Mu&, usize),
        usize index, AccessMode mode, CB cb
    ) const {
        Traversal child;
        child.op = ELEM_FUNC;
        child.elem_func = func;
        child.index = index;
        follow_ref(child, ref, mode, cb);
    }

     // noexcept because any user code called from here should be confirmed to
     // already work without throwing.
    inline Reference to_reference () const noexcept {
        if (addressable) {
            return Pointer(Type(desc, readonly), address);
        }
        else if (op == START) {
            return *reference;
        }
        else {
            Reference parent_ref = parent->to_reference();
            switch (op) {
                case DELEGATE: case ATTR: case ELEM:
                    return parent_ref.chain(acr);
                case ATTR_FUNC:
                    return parent_ref.chain_attr_func(attr_func, *key);
                case ELEM_FUNC:
                    return parent_ref.chain_elem_func(elem_func, index);
                default: never();
            }
        }
    }

    Location to_location () const noexcept {
        if (op == START) {
            if (*location) return location;
             // This * took a half a day of debugging to add. :(
            else return Location(*reference);
        }
        else {
            Location parent_loc = parent->to_location();
            switch (op) {
                case DELEGATE: return parent_loc;
                case ATTR: case ATTR_FUNC:
                    return Location(move(parent_loc), *key);
                case ELEM: case ELEM_FUNC:
                    return Location(move(parent_loc), index);
                default: never();
            }
        }
    }

};

inline const Traversal* Traversal::current_start = null;

} // namespace ayu::in
