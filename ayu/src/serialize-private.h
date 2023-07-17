#pragma once
#include "../serialize.h"

#include "../errors.h"
#include "../reference.h"
#include "../resource.h"
#include "descriptors-private.h"
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
template <class CB>
bool ser_maybe_attr (const Traversal&, const AnyString&, AccessMode, CB);
 // Throws if the attr isn't found
template <class CB>
void ser_attr (const Traversal&, const AnyString&, AccessMode, CB);

 ///// Elem operations
usize ser_get_length (const Traversal&);
 // Implement set_length by counting up used length
void ser_claim_length (const Traversal&, usize& claimed, usize len);
void ser_set_length (const Traversal&, usize);

 // If elem is out of range, returns false and doesn't call the callback
template <class CB>
bool ser_maybe_elem (const Traversal&, usize, AccessMode, CB);
 // Throws if elem is out of bounds
template <class CB>
void ser_elem (const Traversal&, usize, AccessMode, CB);

///// INLINE DEFINITIONS

template <class CB>
bool ser_maybe_attr (
    const Traversal& trav, const AnyString& key,
    AccessMode mode, CB cb
) try {
    if (auto attrs = trav.desc->attrs()) {
         // Note: This will likely be called once for each attr, making it
         // O(N^2) over the number of attrs.  If we want we could optimize for
         // large N by keeping a temporary map...somewhere
         //
         // First check direct attrs
        for (uint i = 0; i < attrs->n_attrs; i++) {
            auto attr = attrs->attr(i);
            if (attr->key == key) {
                trav.follow_attr(attr->acr(), key, mode, cb);
                return true;
            }
        }
         // Then included attrs
        for (uint i = 0; i < attrs->n_attrs; i++) {
            auto attr = attrs->attr(i);
            auto acr = attr->acr();
            if (acr->attr_flags & ATTR_INCLUDE) {
                 // Change mode to modify so we don't clobber the other attrs of
                 // the included item.  Hopefully it won't matter, because
                 // inheriting through a non-addressable reference will be
                 // pretty slow no matter what.  Perhaps if we really wanted to
                 // optimize this, then in claim_keys we could build up a
                 // structure mirroring the inclusion diagram and follow it,
                 // instead of just keeping the flat list of keys.
                 //
                 // TODO: This may not behave properly with only_addressable.
                bool found = false;
                trav.follow_attr(
                    acr, attr->key, mode == ACR_WRITE ? ACR_MODIFY : mode,
                    [&found, &key, mode, &cb](const Traversal& child)
                {
                    found = ser_maybe_attr(child, key, mode, cb);
                });
                if (found) return true;
            }
        }
        [[unlikely]] return false;
    }
    else if (auto attr_func = trav.desc->attr_func()) {
        if (Reference ref = attr_func->f(*trav.address, key)) {
            trav.follow_attr_func(move(ref), attr_func->f, key, mode, cb);
            return true;
        }
        [[unlikely]] return false;
    }
    else if (auto acr = trav.desc->delegate_acr()) {
        bool r = false;
        trav.follow_delegate(
            acr, mode == ACR_WRITE ? ACR_MODIFY : mode,
            [&r, &key, mode, &cb](const Traversal& child)
        {
            r = ser_maybe_attr(child, key, mode, cb);
        });
        return r;
    }
    else throw NoAttrs();
}
catch (const SerializeFailed&) { throw; }
catch (const std::exception&) {
    throw SerializeFailed(
        trav.to_location(), trav.desc, std::current_exception()
    );
}

[[noreturn]] void throw_AttrNotFound (const Traversal&, const AnyString&);

template <class CB>
void ser_attr (
    const Traversal& trav, const AnyString& key, AccessMode mode, CB cb
) try {
    if (!ser_maybe_attr(trav, key, mode, cb)) {
        throw AttrNotFound(key);
    }
}
catch (const SerializeFailed&) { throw; }
catch (const std::exception&) {
    throw SerializeFailed(
        trav.to_location(), trav.desc, std::current_exception()
    );
}

template <class CB>
bool ser_maybe_elem (
    const Traversal& trav, usize index, AccessMode mode, CB cb
) try {
    if (auto elems = trav.desc->elems()) {
        if (index < elems->n_elems) {
            auto acr = elems->elem(index)->acr();
            trav.follow_elem(acr, index, mode, cb);
            return true;
        }
        else [[unlikely]] return false;
    }
    else if (auto elem_func = trav.desc->elem_func()) {
        if (Reference ref = elem_func->f(*trav.address, index)) {
            trav.follow_elem_func(move(ref), elem_func->f, index, mode, cb);
            return true;
        }
        else [[unlikely]] return false;
    }
    else if (auto acr = trav.desc->delegate_acr()) {
        bool found = false;
        trav.follow_delegate(
            acr, mode == ACR_WRITE ? ACR_MODIFY : mode,
            [&found, index, mode, &cb](const Traversal& child)
        {
            found = ser_maybe_elem(child, index, mode, cb);
        });
        return found;
    }
    else throw NoElems();
}
catch (const SerializeFailed&) { throw; }
catch (const std::exception&) {
    throw SerializeFailed(
        trav.to_location(), trav.desc, std::current_exception()
    );
}

[[noreturn]] void throw_ElemNotFound (const Traversal&, usize);

template <class CB>
void ser_elem (
    const Traversal& trav, usize index, AccessMode mode, CB cb
) {
    if (!ser_maybe_elem(trav, index, mode, cb)) {
        throw_ElemNotFound(trav, index);
    }
}

} // namespace ayu::in
