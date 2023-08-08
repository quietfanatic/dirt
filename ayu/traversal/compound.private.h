#pragma once

#include "compound.h"

#include "../reflection/reference.h"
#include "../reflection/descriptors.private.h"
#include "traversal.private.h"

namespace ayu::in {

///// ATTR OPERATIONS
 // Implement get_keys by adding keys to an array of AnyStrings
void trav_collect_keys (const Traversal&, UniqueArray<AnyString>&);

 // Implement set_keys by removing keys from an array
void trav_claim_keys (const Traversal&, UniqueArray<AnyString>&, bool optional);
void trav_set_keys (const Traversal&, UniqueArray<AnyString>&&);

 // If the attr isn't found, returns false and doesn't call the callback
template <class CB>
bool trav_maybe_attr (const Traversal&, const AnyString&, AccessMode, CB);
 // Throws if the attr isn't found
template <class CB>
void trav_attr (const Traversal&, const AnyString&, AccessMode, CB);

 ///// Elem operations
usize trav_get_length (const Traversal&);
 // Implement set_length by counting up used length
void trav_claim_length (const Traversal&, usize& claimed, usize len);
void trav_set_length (const Traversal&, usize);

 // If elem is out of range, returns false and doesn't call the callback
template <class CB>
bool trav_maybe_elem (const Traversal&, usize, AccessMode, CB);
 // Throws if elem is out of bounds
template <class CB>
void trav_elem (const Traversal&, usize, AccessMode, CB);

 ///// Exceptions
[[noreturn]]
void raise_AttrNotFound (Type, const AnyString&);
[[noreturn]]
void raise_ElemNotFound (Type, usize);
[[noreturn]]
void raise_AttrsNotSupported (Type);
[[noreturn]]
void raise_ElemsNotSupported (Type);

///// INLINE DEFINITIONS

template <class CB> NOINLINE
bool trav_maybe_attr_attrs (
    const Traversal& trav, const AnyString& key,
    AccessMode mode, CB cb, const AttrsDcrPrivate* attrs
) {
     // This will likely be called once for each attr, making it O(N^2) over
     // the number of attrs.  If we want we could optimize for large N by
     // keeping a temporary map...somewhere
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
        if (acr->attr_flags & AttrFlags::Include) {
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
                acr, attr->key,
                mode == AccessMode::Write ? AccessMode::Modify : mode,
                [&found, &key, mode, &cb](const Traversal& child)
            {
                found = trav_maybe_attr(child, key, mode, cb);
            });
            if (found) return true;
        }
    }
    [[unlikely]] return false;
}

template <class CB> NOINLINE
bool trav_maybe_attr_attr_func (
    const Traversal& trav, const AnyString& key,
    AccessMode mode, CB cb, AttrFunc<Mu>* f
) {
    if (Reference ref = f(*trav.address, key)) {
        trav.follow_attr_func(move(ref), f, key, mode, cb);
        return true;
    }
    [[unlikely]] return false;
}

template <class CB> NOINLINE
bool trav_maybe_attr_delegate (
    const Traversal& trav, const AnyString& key,
    AccessMode mode, CB cb, const Accessor* acr
) {
    bool r = false;
    trav.follow_delegate(
        acr, mode == AccessMode::Write ? AccessMode::Modify : mode,
        [&r, &key, mode, &cb](const Traversal& child)
    {
        r = trav_maybe_attr(child, key, mode, cb);
    });
    return r;
}

template <class CB> NOINLINE
bool trav_maybe_attr (
    const Traversal& trav, const AnyString& key,
    AccessMode mode, CB cb
) {
    if (auto attrs = trav.desc->attrs()) {
        return trav_maybe_attr_attrs(trav, key, mode, cb, attrs);
    }
    else if (auto attr_func = trav.desc->attr_func()) {
        return trav_maybe_attr_attr_func(trav, key, mode, cb, attr_func->f);
    }
    else if (auto acr = trav.desc->delegate_acr()) {
        return trav_maybe_attr_delegate(trav, key, mode, cb, acr);
    }
    else raise_AttrsNotSupported(trav.desc);
}

template <class CB>
void trav_attr (
    const Traversal& trav, const AnyString& key, AccessMode mode, CB cb
) {
    if (!trav_maybe_attr(trav, key, mode, cb)) {
        raise_AttrNotFound(trav.desc, key);
    }
}

template <class CB> NOINLINE
bool trav_maybe_elem_elems (
    const Traversal& trav, usize index, AccessMode mode, CB cb,
    const ElemsDcrPrivate* elems
) {
    if (index < elems->n_elems) {
        auto acr = elems->elem(index)->acr();
        trav.follow_elem(acr, index, mode, cb);
        return true;
    }
    else [[unlikely]] return false;
}

template <class CB> NOINLINE
bool trav_maybe_elem_elem_func (
    const Traversal& trav, usize index, AccessMode mode, CB cb,
    ElemFunc<Mu>* f
) {
    if (Reference ref = f(*trav.address, index)) {
        trav.follow_elem_func(move(ref), f, index, mode, cb);
        return true;
    }
    else [[unlikely]] return false;
}

template <class CB> NOINLINE
bool trav_maybe_elem_delegate (
    const Traversal& trav, usize index, AccessMode mode, CB cb,
    const Accessor* acr
) {
    bool found = false;
    trav.follow_delegate(
        acr, mode == AccessMode::Write ? AccessMode::Modify : mode,
        [&found, index, mode, &cb](const Traversal& child)
    {
        found = trav_maybe_elem(child, index, mode, cb);
    });
    return found;
}


template <class CB> NOINLINE
bool trav_maybe_elem (
    const Traversal& trav, usize index, AccessMode mode, CB cb
) {
    if (auto elems = trav.desc->elems()) {
        return trav_maybe_elem_elems(trav, index, mode, cb, elems);
    }
    else if (auto elem_func = trav.desc->elem_func()) {
        return trav_maybe_elem_elem_func(trav, index, mode, cb, elem_func->f);
    }
    else if (auto acr = trav.desc->delegate_acr()) {
        return trav_maybe_elem_delegate(trav, index, mode, cb, acr);
    }
    else raise_ElemsNotSupported(trav.desc);
}

template <class CB>
void trav_elem (
    const Traversal& trav, usize index, AccessMode mode, CB cb
) {
    if (!trav_maybe_elem(trav, index, mode, cb)) {
        raise_ElemNotFound(trav.desc, index);
    }
}

} // namespace ayu::in
